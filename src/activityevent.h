#ifndef ACTIVITYEVENT_H
#define ACTIVITYEVENT_H

// Classification shared by activity producers and the activity-log model.
enum class ActivityLevel {
    Info,
    Torrent,
    Warning,
    Error
};

#endif // ACTIVITYEVENT_H
