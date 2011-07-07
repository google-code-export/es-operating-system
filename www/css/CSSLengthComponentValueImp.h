// Generated by esidl (r1745).
// This file is expected to be modified for the Web IDL interface
// implementation.  Permission to use, copy, modify and distribute
// this file in any software license is hereby granted.

#ifndef ORG_W3C_DOM_BOOTSTRAP_CSSLENGTHCOMPONENTVALUEIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_CSSLENGTHCOMPONENTVALUEIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/css/CSSLengthComponentValue.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class CSSLengthComponentValueImp : public ObjectMixin<CSSLengthComponentValueImp>
{
public:
    // CSSLengthComponentValue
    float getEm() __attribute__((weak));
    void setEm(float em) __attribute__((weak));
    float getEx() __attribute__((weak));
    void setEx(float ex) __attribute__((weak));
    float getPx() __attribute__((weak));
    void setPx(float px) __attribute__((weak));
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return css::CSSLengthComponentValue::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return css::CSSLengthComponentValue::getMetaData();
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_CSSLENGTHCOMPONENTVALUEIMP_H_INCLUDED