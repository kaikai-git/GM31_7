#pragma once
#include	<array>
#include	<vector>
#include	<unordered_map>
#include	"../system/commontypes.h"	

struct animationtable {
	int		pictureno;
	int		loopcnt;
};

struct picturetable {
	std::array<Vector2, 4> texuv;
};

class animationsystem {
public:
    static picturetable getPictureUV(
        int& currentIndex,
        int& counter,
        const std::vector<animationtable>& animTbl,
        const std::vector<picturetable>& pictureTbl);
};
