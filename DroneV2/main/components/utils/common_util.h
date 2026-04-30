#include <cmath>
namespace drone{

class NavigationUtils {
public:
    // 하버사인 공식 등을 이용한 두 좌표 사이의 거리(m) 계산
    static float getDistance(double lat1, double lon1, double lat2, double lon2) {
        float dLat = (lat2 - lat1) * M_PI / 180.0;
        float dLon = (lon2 - lon1) * M_PI / 180.0;
        float a = sin(dLat/2) * sin(dLat/2) +
                  cos(lat1*M_PI/180.0) * cos(lat2*M_PI/180.0) *
                  sin(dLon/2) * sin(dLon/2);
        return 6371000.0 * 2 * atan2(sqrt(a), sqrt(1-a));
    }

    // 목표 지점까지의 방위각(Bearing) 계산
    static float getBearing(double lat1, double lon1, double lat2, double lon2) {
        double dLon = (lon2 - lon1) * M_PI / 180.0;
        double y = sin(dLon) * cos(lat2 * M_PI / 180.0);
        double x = cos(lat1 * M_PI / 180.0) * sin(lat2 * M_PI / 180.0) -
                   sin(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) * cos(dLon);
        return atan2(y, x) * 180.0 / M_PI;
    }
};

}