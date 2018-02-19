//
// Created by cpasjuste on 03/02/18.
//

#include "gui_emu.h"
#include "gui_romlist.h"
#include "gui_progressbox.h"

using namespace c2d;

extern int InpMake(Input::Player *players);

extern unsigned char inputServiceSwitch;
extern unsigned char inputP1P2Switch;
extern int nSekCpuCore;

GuiEmu::GuiEmu(Gui *g) : Rectangle(g->getRenderer()->getSize()) {

    ui = g;
    setFillColor(Color::Transparent);

    fpsText = new Text("0123456789", *ui->getSkin()->font, (unsigned int) ui->getFontSize());
    fpsText->setPosition(16, 16);
    add(fpsText);

    setVisibility(C2D_VISIBILITY_HIDDEN);
}

int GuiEmu::run(int driver) {

    bForce60Hz = true;
#if defined(__PSP2__) || defined(__RPI__)
    nSekCpuCore = getSekCpuCore();
#endif
    ///////////
    // AUDIO
    //////////
    nBurnSoundRate = 0;
    if (ui->getConfig()->getValue(Option::Index::ROM_AUDIO, true)) {
#ifdef __NX__
        nBurnSoundRate = 0;
#elif __3DS__
        nBurnSoundRate = 44100;
#else
        nBurnSoundRate = 48000;
#endif
    }
    if (nBurnSoundRate > 0) {
        printf("Creating audio device\n");
        // disable interpolation as it produce "cracking" sound
        // on some games (cps1 (SF2), cave ...)
        nInterpolation = 1;
        nFMInterpolation = 0;
        audio = new C2DAudio(nBurnSoundRate, nBurnFPS);
        if (audio->available) {
            nBurnSoundRate = audio->frequency;
            nBurnSoundLen = audio->buffer_len;
            pBurnSoundOut = audio->buffer;
        }
    }

    if (!audio || !audio->available) {
        printf("Audio disabled\n");
        nBurnSoundRate = 0;
        nBurnSoundLen = 0;
        pBurnSoundOut = NULL;
    }

    ///////////
    // DRIVER
    //////////
    InpInit();
    InpDIP();

    printf("Initialize driver\n");
    if (DrvInit(driver, false) != 0) {
        printf("Driver initialisation failed! Likely causes are:\n"
                       "- Corrupt/Missing ROM(s)\n"
                       "- I/O Error\n"
                       "- Memory error\n\n");
        DrvExit();
        InpExit();
        if (audio) {
            delete (audio);
        }
        ui->getUiProgressBox()->setVisibility(C2D_VISIBILITY_HIDDEN);
        ui->getUiMessageBox()->show("ERROR", "DRIVER INIT FAILED", "OK");
        return -1;
    }

    ///////////
    // VIDEO
    //////////
    printf("Creating video device\n");
    int w, h;
    BurnDrvGetFullSize(&w, &h);
    video = new Video(ui, Vector2f(w, h));
    add(video);
    // set fps text on top
    fpsText->setLayer(1);

    setVisibility(C2D_VISIBILITY_VISIBLE);
    ui->getUiProgressBox()->setVisibility(C2D_VISIBILITY_HIDDEN);
    ui->getUiRomList()->setVisibility(C2D_VISIBILITY_HIDDEN);

    // set per rom input configuration
    ui->updateInputMapping(true);

    // reset
    paused = false;
    nFramesEmulated = 0;
    nFramesRendered = 0;
    nCurrentFrame = 0;
    frame_duration = 1.0f / ((float) nBurnFPS / 100.0f);

    return 0;
}

void GuiEmu::stop() {

    DrvExit();
    InpExit();

    if (video) {
        delete (video);
        video = NULL;
    }

    if (audio) {
        delete (audio);
        audio = NULL;
    }

    setVisibility(C2D_VISIBILITY_HIDDEN);
}

void GuiEmu::pause() {

    paused = true;
    if (audio) {
        audio->Pause(1);
    }
    ui->updateInputMapping(false);
}

void GuiEmu::resume() {

    ui->updateInputMapping(true);
    if (audio) {
        audio->Pause(0);
    }
    paused = false;
#ifdef __NX__
    NXVideo::clear();
#endif
}

void GuiEmu::renderFrame(bool bDraw, int bDrawFps, float fps) {

    fpsText->setVisibility(
            bDrawFps ? C2D_VISIBILITY_VISIBLE : C2D_VISIBILITY_HIDDEN);

    if (!paused) {

        nFramesEmulated++;
        nCurrentFrame++;

        pBurnDraw = NULL;
        if (bDraw) {
            nFramesRendered++;
            video->lock(NULL, (void **) &pBurnDraw, &nBurnPitch);
        }
        BurnDrvFrame();
        if (bDraw) {
            video->unlock();
        }

        if (bDrawFps) {
            sprintf(fpsString, "FPS: %i/%2d", (int) fps, (nBurnFPS / 100));
            fpsText->setString(fpsString);
        }

        if (audio) {
            audio->Play();
        }
    }
}

void GuiEmu::updateFrame() {

    int showFps = ui->getConfig()->getValue(Option::Index::ROM_SHOW_FPS, true);
    int frameSkip = ui->getConfig()->getValue(Option::Index::ROM_FRAMESKIP, true);

    if (frameSkip) {
        bool draw = nFramesEmulated % (frameSkip + 1) == 0;
        renderFrame(draw, showFps, ui->getRenderer()->getFps());
#ifdef __NX__
        ui->getRenderer()->flip(false);
#else
        ui->getRenderer()->flip(draw);
#endif
        float delta = ui->getRenderer()->getDeltaTime().asSeconds();
        if (delta < frame_duration) { // limit fps
            //printf("f: %f | d: %f | m: %f | s: %i\n", frame_duration, delta, frame_duration - delta,
            //       (unsigned int) ((frame_duration - delta) * 1000));
            ui->getRenderer()->delay((unsigned int) ((frame_duration - delta) * 1000));
        }
    } else {
        renderFrame(true, showFps, ui->getRenderer()->getFps());
#ifdef __NX__
        ui->getRenderer()->flip(false);
#else
        ui->getRenderer()->flip();
#endif
    }
}

int GuiEmu::update() {

    inputServiceSwitch = 0;
    inputP1P2Switch = 0;

    int rotation = ui->getConfig()->getValue(Option::Index::ROM_ROTATION, true);
    int rotate = 0;
    if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
        if (rotation == 0) {
            //rotate controls by 90 degrees
            rotate = 1;
        }
        if (rotation == 2) {
            //rotate controls by 270 degrees
            rotate = 3;
        }
    }

    Input::Player *players = ui->getInput()->update(rotate);

    // process menu
    if ((players[0].state & Input::Key::KEY_MENU1)
        && (players[0].state & Input::Key::KEY_MENU2)) {
        pause();
        return UI_KEY_SHOW_MEMU_ROM;
    } else if ((players[0].state & Input::Key::KEY_MENU2)
               && (players[0].state & Input::Key::KEY_FIRE5)) {
        pause();
        return UI_KEY_SHOW_MEMU_STATE;
    } else if ((players[0].state & Input::Key::KEY_MENU2)
               && (players[0].state & Input::Key::KEY_FIRE3)) {
        inputServiceSwitch = 1;
    } else if ((players[0].state & Input::Key::KEY_MENU2)
               && (players[0].state & Input::Key::KEY_FIRE4)) {
        inputP1P2Switch = 1;
    } else if (players[0].state & EV_RESIZE) {
        video->updateScaling();
    }

    InpMake(players);
    updateFrame();

    return 0;
}

Video *GuiEmu::getVideo() {
    return video;
}

#if defined(__PSP2__) || defined(__RPI__)

int GuiEmu::getSekCpuCore() {

    int sekCpuCore = 0; // SEK_CORE_C68K: USE CYCLONE ARM ASM M68K CORE
    // int sekCpuCore = g->GetConfig()->GetRomValue(Option::Index::ROM_M68K);

    std::vector<std::string> zipList;
    int hardware = BurnDrvGetHardwareCode();

    if (!ui->getConfig()->getValue(Option::Index::ROM_NEOBIOS, true)
        && RomList::IsHardware(hardware, HARDWARE_PREFIX_SNK)) {
        sekCpuCore = 1; // SEK_CORE_M68K: USE C M68K CORE
        ui->getUiMessageBox()->show("WARNING", "UNIBIOS DOESNT SUPPORT THE M68K ASM CORE\n"
                "CYCLONE ASM CORE DISABLED", "OK");
    }

    if (RomList::IsHardware(hardware, HARDWARE_PREFIX_SEGA)) {
        if (hardware & HARDWARE_SEGA_FD1089A_ENC
            || hardware & HARDWARE_SEGA_FD1089B_ENC
            || hardware & HARDWARE_SEGA_MC8123_ENC
            || hardware & HARDWARE_SEGA_FD1094_ENC
            || hardware & HARDWARE_SEGA_FD1094_ENC_CPU2) {
            sekCpuCore = 1; // SEK_CORE_M68K: USE C M68K CORE
            ui->getUiMessageBox()->show("WARNING", "ROM IS CRYPTED, USE DECRYPTED ROM (CLONE)\n"
                    "TO ENABLE CYCLONE ASM CORE (FASTER)", "OK");
        }
    } else if (RomList::IsHardware(hardware, HARDWARE_PREFIX_TOAPLAN)) {
        zipList.push_back("batrider");
        zipList.push_back("bbakraid");
        zipList.push_back("bgaregga");
    } else if (RomList::IsHardware(hardware, HARDWARE_PREFIX_SNK)) {
        zipList.push_back("kof97");
        zipList.push_back("kof98");
        zipList.push_back("kof99");
        zipList.push_back("kof2000");
        zipList.push_back("kof2001");
        zipList.push_back("kof2002");
        zipList.push_back("kf2k3pcb");
        //zipList.push_back("kof2003"); // WORKS
    }

    std::string zip = BurnDrvGetTextA(DRV_NAME);
    for (unsigned int i = 0; i < zipList.size(); i++) {
        if (zipList[i].compare(0, zip.length(), zip) == 0) {
            ui->getUiMessageBox()->show("WARNING", "THIS ROM DOESNT SUPPORT THE M68K ASM CORE\n"
                    "CYCLONE ASM CORE DISABLED", "OK");
            sekCpuCore = 1; // SEK_CORE_M68K: USE C M68K CORE
            break;
        }
    }

    zipList.clear();

    return sekCpuCore;
}

#endif
