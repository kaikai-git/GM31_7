#include   <vector>
#include	"animationsystem.h"

// 現在のUV情報を取得する
picturetable animationsystem::getPictureUV(
    int& currentIndex,
    int& counter,
    const std::vector<animationtable>& animTbl,
    const std::vector<picturetable>& pictureTbl)
{

    const auto& anim = animTbl[currentIndex];

    if (counter >= anim.loopcnt) {
        currentIndex++;
        counter = 0;

        // 終端チェック（-1 または サイズオーバーでループ）
        if (currentIndex >= animTbl.size()||
            animTbl[currentIndex].pictureno == -1) {
            currentIndex = 0;
        }
    }
    counter++;

    int pictureIdx = animTbl[currentIndex].pictureno;
    return pictureTbl[pictureIdx];
}
