// Generated by esidl (r1745).
// This file is expected to be modified for the Web IDL interface
// implementation.  Permission to use, copy, modify and distribute
// this file in any software license is hereby granted.

#ifndef ORG_W3C_DOM_BOOTSTRAP_HTMLLIELEMENTIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_HTMLLIELEMENTIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/html/HTMLLIElement.h>
#include "HTMLElementImp.h"

#include <org/w3c/dom/html/HTMLElement.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class HTMLLIElementImp : public ObjectMixin<HTMLLIElementImp, HTMLElementImp>
{
public:
    // HTMLLIElement
    int getValue() __attribute__((weak));
    void setValue(int value) __attribute__((weak));
    // HTMLLIElement-20
    std::u16string getType() __attribute__((weak));
    void setType(std::u16string type) __attribute__((weak));
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return html::HTMLLIElement::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return html::HTMLLIElement::getMetaData();
    }
    HTMLLIElementImp(DocumentImp* ownerDocument) :
        ObjectMixin(ownerDocument, u"li") {
    }
    HTMLLIElementImp(HTMLLIElementImp* org, bool deep) :
        ObjectMixin(org, deep) {
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_HTMLLIELEMENTIMP_H_INCLUDED