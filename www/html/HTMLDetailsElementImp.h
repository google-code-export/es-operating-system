// Generated by esidl (r1745).
// This file is expected to be modified for the Web IDL interface
// implementation.  Permission to use, copy, modify and distribute
// this file in any software license is hereby granted.

#ifndef ORG_W3C_DOM_BOOTSTRAP_HTMLDETAILSELEMENTIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_HTMLDETAILSELEMENTIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/html/HTMLDetailsElement.h>
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
class HTMLDetailsElementImp : public ObjectMixin<HTMLDetailsElementImp, HTMLElementImp>
{
public:
    // HTMLDetailsElement
    bool getOpen() __attribute__((weak));
    void setOpen(bool open) __attribute__((weak));
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return html::HTMLDetailsElement::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return html::HTMLDetailsElement::getMetaData();
    }
    HTMLDetailsElementImp(DocumentImp* ownerDocument) :
        ObjectMixin(ownerDocument, u"details") {
    }
    HTMLDetailsElementImp(HTMLDetailsElementImp* org, bool deep) :
        ObjectMixin(org, deep) {
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_HTMLDETAILSELEMENTIMP_H_INCLUDED