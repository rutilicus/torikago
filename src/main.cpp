#include <fstream>
#include <vector>
#include <algorithm>
#include <memory>

#include "DxLib.h"
#include "nlohmann/json.hpp"

#include "../include/message.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    constexpr int FPS = 60;
    constexpr int TEXT_DRAW_BUF = DEFAULT_FONT_SIZE / 2;

    // ��ʃT�C�Y�i�[�ϐ�
    RECT windowRect;

    // ��ʃT�C�Y�擾
    if (GetWindowRect(GetDesktopWindow(), &windowRect) == FALSE) {
        return -1;
    }

    int screenWidth = windowRect.right - windowRect.left;
    int screenHeight = windowRect.bottom - windowRect.top;

    // �ݒ�JSON�ǂݍ���
    std::ifstream ifs("data/config.json");
    if (!ifs.is_open()) {
        return -1;
    }
    nlohmann::json configJson;
    try {
        ifs >> configJson;
    } catch (...) {
        return -1;
    }
    ifs.close();

    // ������b�Z�[�W�ꗗJSON�ǂݍ���
    ifs.open("data/timer.json");
    if (!ifs.is_open()) {
        return -1;
    }
    nlohmann::json timerJson;
    try {
        ifs >> timerJson;
    } catch (...) {
        return -1;
    }
    ifs.close();

    std::vector<std::shared_ptr<Message>> timerMessages;
    for (nlohmann::json::iterator itr = timerJson.begin(); itr != timerJson.end(); itr++) {
        if (itr->contains("time") && itr->contains("messages")) {
            int time = std::max<int>((*itr)["time"].get<int>(), 1);
            // ���ɓ���time�œo�^����Ă���ꍇ�͂��̗v�f�ɒǉ��A�o�^����Ă��Ȃ��ꍇ�͐V�����C���X�^���X�𐶐�
            auto findRes = std::find_if(
                timerMessages.begin(), timerMessages.end(),
                [time](std::shared_ptr<Message> elem) { return elem->getTime() == time; });
            std::shared_ptr<Message> addElem;
            if (findRes == timerMessages.end()) {
                addElem = std::shared_ptr<Message>(new Message(time));
                timerMessages.push_back(addElem);
            } else {
                addElem = *findRes;
            }
            for (nlohmann::json::iterator mItr = (*itr)["messages"].begin(); mItr != (*itr)["messages"].end(); mItr++) {
                if (mItr->contains("message")) {
                    addElem->addMessage((*mItr)["message"].get<std::string>());
                }
            }
        }
    }
    // ������b�Z�[�W�ꗗ�͕\���Ԋu�ō~���\�[�g����
    std::sort(
        timerMessages.begin(), timerMessages.end(),
        [](std::shared_ptr<Message> lhs, std::shared_ptr<Message> rhs) {return lhs->getTime() > rhs->getTime(); });

    // �N���b�N�����b�Z�[�W�ꗗJSON�ǂݍ���
    ifs.open("data/click.json");
    if (!ifs.is_open()) {
        return -1;
    }
    nlohmann::json clickJson;
    try {
        ifs >> clickJson;
    }
    catch (...) {
        return -1;
    }
    ifs.close();

    Message clickMessage(0);
    if (clickJson.contains("messages")) {
        for (nlohmann::json::iterator itr = clickJson["messages"].begin(); itr != clickJson["messages"].end(); itr++) {
            if (itr->contains("message")) {
                clickMessage.addMessage((*itr)["message"].get<std::string>());
            }
        }
    }

    // ���O�o�͗}��
    SetOutApplicationLogValidFlag(FALSE);

    // �擾������ʃT�C�Y�ɕύX
    SetGraphMode(screenWidth, screenHeight, 32);

    // ��ʔz�u�ʒu������ɐݒ�
    SetWindowPosition(0, 0);

    // �g�p���镶���R�[�h�� UTF-8 �ɐݒ�
    SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);

    // �E�C���h�E���[�h�ŋN��
    ChangeWindowMode(TRUE);

    // ���߃E�C���h�E�ݒ�
    SetUseUpdateLayerdWindowFlag(TRUE);

    // �E�C���h�E�͏�ɍőO�ʂŏ펞�A�N�e�B�u
    SetWindowZOrder(DX_WIN_ZTYPE_TOPMOST);
    SetAlwaysRunFlag(TRUE);

    // �E�C���h�E�^�C�g���ݒ�
    SetMainWindowText("torikago");

    // DxLib������
    if (DxLib_Init() == -1)
    {
        return -1;
    }

    // �`��Ώۂɂł���A���t�@�`�����l���t����ʂ��쐬
    int screen = MakeScreen(screenWidth, screenHeight, TRUE);

    // ��ʎ�荞�ݗp�̃\�t�g�E�G�A�p�摜���쐬
    int softImage = MakeARGB8ColorSoftImage(screenWidth, screenHeight);

    // �摜��ǂݍ��ލۂɃA���t�@�l��RGB�l�ɏ�Z����悤�ɐݒ肷��
    SetUsePremulAlphaConvertLoad(TRUE);

    // �摜�擾
    int grHandle = 0;
    int grWidth = 0;
    int grHeight = 0;
    if (configJson.contains("image")) {
        grHandle = LoadGraph(configJson["image"].get<std::string>().c_str());
        if (grHandle != -1) {
            if (GetGraphSize(grHandle, &grWidth, &grHeight) == -1) {
                return -1;
            }
        }
    } else {
        return -1;
    }

    // �摜���W(���S���W)
    int x = screenWidth / 2;
    int y = screenHeight / 2;

    // �`��{��
    double drawRate = configJson.value("initRate", 0.25);

    // �`��F�擾
    const int bgColorR = configJson.value("bgColorR", 0);
    const int bgColorG = configJson.value("bgColorG", 0);
    const int bgColorB = configJson.value("bgColorB", 0);
    const int textColorR = configJson.value("textColorR", 0);
    const int textColorG = configJson.value("textColorG", 0);
    const int textColorB = configJson.value("textColorB", 0);

    // �e�L�X�g�\������(�~���b)�擾
    const int textDispTime = configJson.value("textDispTime", 15000);

    // �}�E�X�N���b�N���
    bool mouseDown = false;
    int clickX = -1;
    int clickY = -1;
    int initX = -1;
    int initY = -1;

    // �O��`��G���A
    int prevDrawX = 0;
    int prevDrawY = 0;
    int prevDrawWidth = 0;
    int prevDrawHeight = 0;
    bool initDraw = false;

    // ���[�v�J�n�������L�^
    int startTime = GetNowCount() - 1;

    // �\����������
    std::string text;
    int prevDispTime = -1;
    int prevTextDrawX = 0;
    int prevTextDrawY = 0;
    int prevTextDrawWidth = 0;
    int prevTextDrawHeight = 0;
    bool prevTextDraw = false;

    // ���C�����[�v
    while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0)
    {
        // ���ݎ������擾
        int passTime = GetNowCount() - startTime;

        // ��ʂ��N���A
        ClearDrawScreen();

        // �}�E�X�����擾
        int xTmp = 0;
        int yTmp = 0;
        int type = 0;
        int button = 0;
        bool clicked = false;
        bool stateChange = false;
        // �}�E�X�̓��̓o�b�t�@�����ׂď���
        while (GetMouseInputLog2(&button, &xTmp, &yTmp, &type) == 0) {
            if (button & MOUSE_INPUT_LEFT) {
                if (type == MOUSE_INPUT_LOG_DOWN) {
                    // �}�E�X�������ꂽ�̂ŁA���݈ʒu���N���b�N�ӏ��ɐݒ�
                    clickX = xTmp;
                    clickY = yTmp;
                    initX = clickX;
                    initY = clickY;
                    mouseDown = true;
                } else {
                    if (mouseDown) {
                        if (initX == xTmp && initY == yTmp) {
                            // �������W�̏ꍇ�̓N���b�N����
                            clicked = true;
                        } else {
                            // ���W���قȂ�ꍇ�͕`���ԕύX
                            stateChange = true;
                        }
                        x += xTmp - clickX;
                        y += yTmp - clickY;
                    }
                    mouseDown = false;
                }
            }
        }

        // �}�E�X�������͍��W�ړ������{
        if (mouseDown) {
            if (GetMousePoint(&xTmp, &yTmp) == 0) {
                if (xTmp != clickX || yTmp != clickY) {
                    stateChange = true;
                }
                x += xTmp - clickX;
                y += yTmp - clickY;
                clickX = xTmp;
                clickY = yTmp;
            }
        }

        // �N���b�N���̓e�L�X�g�`�撆�Ȃ�O��`�掞�Ԃ�ύX���ăe�L�X�g���\���ɂ���
        // �e�L�X�g��\���Ȃ�N���b�N���e�L�X�g���璊�I���ĕ\��������
        if (clicked) {
            if (prevTextDraw) {
                prevDispTime = passTime - textDispTime - 1;
            } else {
                text = clickMessage.getMessage();
                prevDispTime = passTime;
                stateChange = true;
            }
        }

        // ����܂��͕`���ԕύX������ꍇ�͉摜�`�揈�������{
        if (!initDraw || stateChange) {

            // �`��u�����h���[�h����Z�ς݃A���t�@�ɃZ�b�g
            SetDrawBlendMode(DX_BLENDMODE_PMA_ALPHA, 255);

            // �O��`��G���A���N���A
            if (!initDraw) {
                ClearRectSoftImage(softImage, 0, 0, screenWidth, screenHeight);
            } else {
                ClearRectSoftImage(softImage, prevDrawX, prevDrawY, prevDrawWidth, prevDrawHeight);
            }
            if (prevTextDraw) {
                ClearRectSoftImage(softImage, prevTextDrawX, prevTextDrawY, prevTextDrawWidth, prevTextDrawHeight);
            }

            // �摜��`��
            DrawRotaGraph(x, y, drawRate, 0, grHandle, TRUE);

            // �`��G���A��ݒ�̏�A�`���̉摜���\�t�g�C���[�W�Ɏ擾����
            // ��ʓ��Ɏ擾�͈͂����܂�悤����
            prevDrawX = x - grWidth * drawRate / 2;
            prevDrawY = y - grHeight * drawRate / 2;
            prevDrawWidth = grWidth * drawRate;
            prevDrawHeight = grHeight * drawRate;
            if (prevDrawX + prevDrawWidth >= screenWidth) {
                prevDrawWidth = screenWidth - prevDrawX;
            }
            if (prevDrawY + prevDrawHeight >= screenHeight) {
                prevDrawHeight = screenHeight - prevDrawY;
            }
            if (prevDrawX < 0) {
                prevDrawWidth += prevDrawX;
                prevDrawX = 0;
            }
            if (prevDrawY < 0) {
                prevDrawHeight += prevDrawY;
                prevDrawY = 0;
            }
            GetDrawScreenSoftImageDestPos(
                prevDrawX, prevDrawY, prevDrawX + prevDrawWidth, prevDrawY + prevDrawHeight,
                softImage, prevDrawX, prevDrawY);
        }

        // ������`�撆�łȂ��ꍇ�͕\�����I�����{
        if (!prevTextDraw) {
            int dispSetTime = -1;
            for (auto itr = timerMessages.begin(); itr != timerMessages.end(); itr++) {
                if (dispSetTime != -1) {
                    (*itr)->setLastTime(dispSetTime / (*itr)->getTime() * (*itr)->getTime());
                } else if (passTime - (*itr)->getLastTime() > (*itr)->getTime()) {
                    dispSetTime = passTime / (*itr)->getTime() * (*itr)->getTime();
                    (*itr)->setLastTime(dispSetTime);
                    text = (*itr)->getMessage();
                    prevDispTime = dispSetTime;
                    stateChange = true;
                }
            }
        }

        // ������\�����ԓ��A���摜�ω�������ꍇ�͕�����`�揈��
        if (!text.empty() && passTime - prevDispTime <= textDispTime && (!initDraw || stateChange)) {
            int textAreaWidth = 0;
            int textAreaHeight = 0;
            int textLine = 0;
            GetDrawStringSize(&textAreaWidth, &textAreaHeight, &textLine, text.c_str(), strlen(text.c_str()));

            prevTextDrawWidth = textAreaWidth + TEXT_DRAW_BUF * 2;
            prevTextDrawX = x - textAreaWidth / 2 - TEXT_DRAW_BUF;
            prevTextDrawHeight = textAreaHeight + TEXT_DRAW_BUF * 2;
            prevTextDrawY = prevDrawY - prevTextDrawHeight;

            // �g��`���Ƀe�L�X�g�`��
            DrawRoundRect(
                prevTextDrawX, prevTextDrawY, prevTextDrawX + prevTextDrawWidth, prevTextDrawY + prevTextDrawHeight,
                TEXT_DRAW_BUF, TEXT_DRAW_BUF, RGB(bgColorR, bgColorG, bgColorB), TRUE);
            DrawString(
                prevTextDrawX + TEXT_DRAW_BUF, prevTextDrawY + TEXT_DRAW_BUF,
                text.c_str(), RGB(textColorR, textColorG, textColorB));

            if (prevTextDrawX + prevTextDrawWidth >= screenWidth) {
                prevTextDrawWidth = screenWidth - prevTextDrawX;
            }
            if (prevTextDrawY + prevTextDrawHeight >= screenHeight) {
                prevTextDrawHeight = screenHeight - prevTextDrawY;
            }
            if (prevTextDrawX < 0) {
                prevDrawWidth += prevTextDrawX;
                prevTextDrawX = 0;
            }
            if (prevTextDrawY < 0) {
                prevDrawHeight += prevTextDrawY;
                prevTextDrawY = 0;
            }

            GetDrawScreenSoftImageDestPos(
                prevTextDrawX, prevTextDrawY,
                prevTextDrawX + prevTextDrawWidth, prevTextDrawY + prevTextDrawHeight,
                softImage, prevTextDrawX, prevTextDrawY);
            prevTextDraw = true;
        } else if (passTime - prevDispTime > textDispTime) {
            // �\�����Ԍo�߂̏ꍇ�͔�\�������Ƃ���
            // �摜�`��ɕω����Ȃ��ꍇ��else�ɓ��邽�ߎ��Ԃ𔻒�����ɒǉ�

            // �������`��̏ꍇ�ł��A�O�t���[���ŕ`�悪�������ꍇ�͗̈�N���A�����{
            if (prevTextDraw) {
                ClearRectSoftImage(softImage, prevTextDrawX, prevTextDrawY, prevTextDrawWidth, prevTextDrawHeight);
                stateChange = true;
            }

            prevTextDraw = false;
        }

        // ��荞�񂾃\�t�g�C���[�W���g�p���ē��߃E�C���h�E�̏�Ԃ��X�V����
        // �`��ύX������ꍇ�̂ݎ��{
        if (!initDraw || stateChange) {
            UpdateLayerdWindowForPremultipliedAlphaSoftImage(softImage);
        }

        initDraw = true;

        // wait
        Sleep(1000 / FPS);
    }

    // DxLib�I������
    DxLib_End();

    return 0;
}
