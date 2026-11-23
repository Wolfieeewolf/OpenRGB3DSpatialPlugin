// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SENSITIVEFIELDWATCH_H
#define SENSITIVEFIELDWATCH_H

#include <functional>
#include <memory>

/* Reports when keyboard focus is in a password field, so Reactive can ignore the keys typed
   there. Only the focused element's "is password" flag is read, never its text.

     Windows: UI Automation focus events (IsPassword).
     Linux:   AT-SPI focus events (password-text role). Only applications that expose
              accessibility report this; most GTK, Qt and browser windows do.
     macOS:   not needed; the system blocks key reading in secure fields itself.

   Payment-card and other sensitive fields are ordinary text fields to the system, so they
   cannot be told apart. Start() and Stop() are called on the GUI thread. */
class SensitiveFieldWatch
{
public:
    using Callback = std::function<void(bool in_password_field)>;

    SensitiveFieldWatch();
    ~SensitiveFieldWatch();

    void Start(Callback on_change);
    void Stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
