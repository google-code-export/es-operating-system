// Generated by esidl (r1752).
// This file is expected to be modified for the Web IDL interface
// implementation.  Permission to use, copy, modify and distribute
// this file in any software license is hereby granted.

#ifndef ORG_W3C_DOM_BOOTSTRAP_NAVIGATORIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_NAVIGATORIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/html/Navigator.h>

#include <org/w3c/dom/html/NavigatorUserMediaSuccessCallback.h>
#include <org/w3c/dom/html/NavigatorUserMediaErrorCallback.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class NavigatorImp : public ObjectMixin<NavigatorImp>
{
public:
    // Navigator
    // NavigatorID
    std::u16string getAppName() __attribute__((weak));
    std::u16string getAppVersion() __attribute__((weak));
    std::u16string getPlatform() __attribute__((weak));
    std::u16string getUserAgent() __attribute__((weak));
    // NavigatorOnLine
    bool getOnLine() __attribute__((weak));
    // NavigatorContentUtils
    void registerProtocolHandler(std::u16string scheme, std::u16string url, std::u16string title) __attribute__((weak));
    void registerContentHandler(std::u16string mimeType, std::u16string url, std::u16string title) __attribute__((weak));
    // NavigatorStorageUtils
    void yieldForStorageUpdates() __attribute__((weak));
    // NavigatorUserMedia
    void getUserMedia(std::u16string options, html::NavigatorUserMediaSuccessCallback successCallback) __attribute__((weak));
    void getUserMedia(std::u16string options, html::NavigatorUserMediaSuccessCallback successCallback, html::NavigatorUserMediaErrorCallback errorCallback) __attribute__((weak));
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return html::Navigator::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return html::Navigator::getMetaData();
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_NAVIGATORIMP_H_INCLUDED