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

    // 画面サイズ格納変数
    RECT windowRect;

    // 画面サイズ取得
    if (GetWindowRect(GetDesktopWindow(), &windowRect) == FALSE) {
        return -1;
    }

    int screenWidth = windowRect.right - windowRect.left;
    int screenHeight = windowRect.bottom - windowRect.top;

    // 設定JSON読み込み
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

    // 定期メッセージ一覧JSON読み込み
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
            // 既に同じtimeで登録されている場合はその要素に追加、登録されていない場合は新しいインスタンスを生成
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
    // 定期メッセージ一覧は表示間隔で降順ソートする
    std::sort(
        timerMessages.begin(), timerMessages.end(),
        [](std::shared_ptr<Message> lhs, std::shared_ptr<Message> rhs) {return lhs->getTime() > rhs->getTime(); });

    // クリック時メッセージ一覧JSON読み込み
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

    // ログ出力抑制
    SetOutApplicationLogValidFlag(FALSE);

    // 取得した画面サイズに変更
    SetGraphMode(screenWidth, screenHeight, 32);

    // 画面配置位置を左上に設定
    SetWindowPosition(0, 0);

    // 使用する文字コードを UTF-8 に設定
    SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);

    // ウインドウモードで起動
    ChangeWindowMode(TRUE);

    // 透過ウインドウ設定
    SetUseUpdateLayerdWindowFlag(TRUE);

    // ウインドウは常に最前面で常時アクティブ
    SetWindowZOrder(DX_WIN_ZTYPE_TOPMOST);
    SetAlwaysRunFlag(TRUE);

    // ウインドウタイトル設定
    SetMainWindowText("torikago");

    // DxLib初期化
    if (DxLib_Init() == -1)
    {
        return -1;
    }

    // 描画対象にできるアルファチャンネル付き画面を作成
    int screen = MakeScreen(screenWidth, screenHeight, TRUE);

    // 画面取り込み用のソフトウエア用画像を作成
    int softImage = MakeARGB8ColorSoftImage(screenWidth, screenHeight);

    // 画像を読み込む際にアルファ値をRGB値に乗算するように設定する
    SetUsePremulAlphaConvertLoad(TRUE);

    // 画像取得
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

    // 画像座標(中心座標)
    int x = screenWidth / 2;
    int y = screenHeight / 2;

    // 描画倍率
    double drawRate = configJson.value("initRate", 0.25);

    // 描画色取得
    const int bgColorR = configJson.value("bgColorR", 0);
    const int bgColorG = configJson.value("bgColorG", 0);
    const int bgColorB = configJson.value("bgColorB", 0);
    const int textColorR = configJson.value("textColorR", 0);
    const int textColorG = configJson.value("textColorG", 0);
    const int textColorB = configJson.value("textColorB", 0);

    // テキスト表示時間(ミリ秒)取得
    const int textDispTime = configJson.value("textDispTime", 15000);

    // マウスクリック状態
    bool mouseDown = false;
    int clickX = -1;
    int clickY = -1;
    int initX = -1;
    int initY = -1;

    // 前回描画エリア
    int prevDrawX = 0;
    int prevDrawY = 0;
    int prevDrawWidth = 0;
    int prevDrawHeight = 0;
    bool initDraw = false;

    // ループ開始時刻を記録
    int startTime = GetNowCount() - 1;

    // 表示文字列情報
    std::string text;
    int prevDispTime = -1;
    int prevTextDrawX = 0;
    int prevTextDrawY = 0;
    int prevTextDrawWidth = 0;
    int prevTextDrawHeight = 0;
    bool prevTextDraw = false;

    // メインループ
    while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0)
    {
        // 現在時刻を取得
        int passTime = GetNowCount() - startTime;

        // 画面をクリア
        ClearDrawScreen();

        // マウス情報を取得
        int xTmp = 0;
        int yTmp = 0;
        int type = 0;
        int button = 0;
        bool clicked = false;
        bool stateChange = false;
        // マウスの入力バッファをすべて処理
        while (GetMouseInputLog2(&button, &xTmp, &yTmp, &type) == 0) {
            if (button & MOUSE_INPUT_LEFT) {
                if (type == MOUSE_INPUT_LOG_DOWN) {
                    // マウスが押されたので、現在位置をクリック箇所に設定
                    clickX = xTmp;
                    clickY = yTmp;
                    initX = clickX;
                    initY = clickY;
                    mouseDown = true;
                } else {
                    if (mouseDown) {
                        if (initX == xTmp && initY == yTmp) {
                            // 同じ座標の場合はクリック扱い
                            clicked = true;
                        } else {
                            // 座標が異なる場合は描画状態変更
                            stateChange = true;
                        }
                        x += xTmp - clickX;
                        y += yTmp - clickY;
                    }
                    mouseDown = false;
                }
            }
        }

        // マウス押下中は座標移動を実施
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

        // クリック時はテキスト描画中なら前回描画時間を変更してテキストを非表示にする
        // テキスト非表示ならクリック時テキストから抽選して表示させる
        if (clicked) {
            if (prevTextDraw) {
                prevDispTime = passTime - textDispTime - 1;
            } else {
                text = clickMessage.getMessage();
                prevDispTime = passTime;
                stateChange = true;
            }
        }

        // 初回または描画状態変更がある場合は画像描画処理を実施
        if (!initDraw || stateChange) {

            // 描画ブレンドモードを乗算済みアルファにセット
            SetDrawBlendMode(DX_BLENDMODE_PMA_ALPHA, 255);

            // 前回描画エリアをクリア
            if (!initDraw) {
                ClearRectSoftImage(softImage, 0, 0, screenWidth, screenHeight);
            } else {
                ClearRectSoftImage(softImage, prevDrawX, prevDrawY, prevDrawWidth, prevDrawHeight);
            }
            if (prevTextDraw) {
                ClearRectSoftImage(softImage, prevTextDrawX, prevTextDrawY, prevTextDrawWidth, prevTextDrawHeight);
            }

            // 画像を描画
            DrawRotaGraph(x, y, drawRate, 0, grHandle, TRUE);

            // 描画エリアを設定の上、描画先の画像をソフトイメージに取得する
            // 画面内に取得範囲が収まるよう調整
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

        // 文字列描画中でない場合は表示抽選を実施
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

        // 文字列表示時間内、かつ画像変化がある場合は文字列描画処理
        if (!text.empty() && passTime - prevDispTime <= textDispTime && (!initDraw || stateChange)) {
            int textAreaWidth = 0;
            int textAreaHeight = 0;
            int textLine = 0;
            GetDrawStringSize(&textAreaWidth, &textAreaHeight, &textLine, text.c_str(), strlen(text.c_str()));

            prevTextDrawWidth = textAreaWidth + TEXT_DRAW_BUF * 2;
            prevTextDrawX = x - textAreaWidth / 2 - TEXT_DRAW_BUF;
            prevTextDrawHeight = textAreaHeight + TEXT_DRAW_BUF * 2;
            prevTextDrawY = prevDrawY - prevTextDrawHeight;

            // 枠を描画後にテキスト描画
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
            // 表示時間経過の場合は非表示扱いとする
            // 画像描画に変化がない場合もelseに入るため時間を判定条件に追加

            // 文字列非描画の場合でも、前フレームで描画があった場合は領域クリアを実施
            if (prevTextDraw) {
                ClearRectSoftImage(softImage, prevTextDrawX, prevTextDrawY, prevTextDrawWidth, prevTextDrawHeight);
                stateChange = true;
            }

            prevTextDraw = false;
        }

        // 取り込んだソフトイメージを使用して透過ウインドウの状態を更新する
        // 描画変更がある場合のみ実施
        if (!initDraw || stateChange) {
            UpdateLayerdWindowForPremultipliedAlphaSoftImage(softImage);
        }

        initDraw = true;

        // wait
        Sleep(1000 / FPS);
    }

    // DxLib終了処理
    DxLib_End();

    return 0;
}
