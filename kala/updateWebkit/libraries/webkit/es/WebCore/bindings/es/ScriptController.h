/*
 * Copyright (c) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptController_h
#define ScriptController_h

#include "ScriptInstance.h"
#include "ScriptValue.h"

namespace WebCore {
    class Event;
    class Frame;
    class HTMLPlugInElement;
    class ScriptSourceCode;
    class ScriptState;
    class String;
    class Widget;
    class XSSAuditor;

    class ScriptController {
    public:
        ScriptController(Frame*);
        ~ScriptController();

        ScriptValue evaluate(const ScriptSourceCode&);
        bool haveWindowShell() const;
        ScriptController* windowShell();
        bool isEnabled() const;
        void setEventHandlerLineNumber(int lineNumber);
        PassScriptInstance createScriptInstanceForWidget(Widget*);
        void disconnectFrame();
        void attachDebugger(void*);
        bool processingUserGesture() const;
        bool isPaused() const;
        const String* sourceURL() const;
        void clearWindowShell();
        void updateDocument();
        void updateSecurityOrigin();
        void clearScriptObjects();
        void updatePlatformScriptObjects();

	XSSAuditor* xssAuditor() { return m_XSSAuditor.get(); }

    private:
        Frame* m_frame;

	OwnPtr<XSSAuditor> m_XSSAuditor;
    };

} // namespace WebCore

#endif // ScriptController_h
