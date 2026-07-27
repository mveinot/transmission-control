#ifndef APPLICATIONAPPEARANCE_H
#define APPLICATIONAPPEARANCE_H

#include <QString>

namespace ApplicationAppearance {

inline constexpr const char *FollowSystem = "system";
inline constexpr const char *Light = "light";
inline constexpr const char *Dark = "dark";

// Applies the requested Qt color scheme process-wide. Invalid or unsupported
// stored values deliberately fall back to the platform appearance.
void apply(const QString &appearance);

}

#endif // APPLICATIONAPPEARANCE_H
