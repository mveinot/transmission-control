#ifndef MACAPPLICATIONBACKEND_H
#define MACAPPLICATIONBACKEND_H

// Changes whether macOS presents Planetary as a regular Dock application.
// Callers must restore regular mode before showing an application window.
bool setMacApplicationDockIconVisible(bool visible);

#endif // MACAPPLICATIONBACKEND_H
