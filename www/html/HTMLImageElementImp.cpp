/*
 * Copyright 2010, 2011 Esrille Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HTMLImageElementImp.h"

namespace org { namespace w3c { namespace dom { namespace bootstrap {

// Node
Node HTMLImageElementImp::cloneNode(bool deep)
{
    return new(std::nothrow) HTMLImageElementImp(this, deep);
}

// HTMLImageElement
std::u16string HTMLImageElementImp::getAlt()
{
    // TODO: implement me!
    return u"";
}

void HTMLImageElementImp::setAlt(std::u16string alt)
{
    // TODO: implement me!
}


std::u16string HTMLImageElementImp::getSrc()
{
    Nullable<std::u16string> src = getAttribute(u"src");
    if (src.hasValue())
        return src.value();
    return u"";
}

void HTMLImageElementImp::setSrc(std::u16string src)
{
    // TODO: implement me!
}

std::u16string HTMLImageElementImp::getUseMap()
{
    // TODO: implement me!
    return u"";
}

void HTMLImageElementImp::setUseMap(std::u16string useMap)
{
    // TODO: implement me!
}

bool HTMLImageElementImp::getIsMap()
{
    // TODO: implement me!
    return 0;
}

void HTMLImageElementImp::setIsMap(bool isMap)
{
    // TODO: implement me!
}

unsigned int HTMLImageElementImp::getWidth()
{
    // TODO: implement me!
    return 0;
}

void HTMLImageElementImp::setWidth(unsigned int width)
{
    // TODO: implement me!
}

unsigned int HTMLImageElementImp::getHeight()
{
    // TODO: implement me!
    return 0;
}

void HTMLImageElementImp::setHeight(unsigned int height)
{
    // TODO: implement me!
}

unsigned int HTMLImageElementImp::getNaturalWidth()
{
    // TODO: implement me!
    return 0;
}

unsigned int HTMLImageElementImp::getNaturalHeight()
{
    // TODO: implement me!
    return 0;
}

bool HTMLImageElementImp::getComplete()
{
    // TODO: implement me!
    return 0;
}

std::u16string HTMLImageElementImp::getName()
{
    // TODO: implement me!
    return u"";
}

void HTMLImageElementImp::setName(std::u16string name)
{
    // TODO: implement me!
}

std::u16string HTMLImageElementImp::getAlign()
{
    // TODO: implement me!
    return u"";
}

void HTMLImageElementImp::setAlign(std::u16string align)
{
    // TODO: implement me!
}

std::u16string HTMLImageElementImp::getBorder()
{
    // TODO: implement me!
    return u"";
}

void HTMLImageElementImp::setBorder(std::u16string border)
{
    // TODO: implement me!
}

unsigned int HTMLImageElementImp::getHspace()
{
    // TODO: implement me!
    return 0;
}

void HTMLImageElementImp::setHspace(unsigned int hspace)
{
    // TODO: implement me!
}

std::u16string HTMLImageElementImp::getLongDesc()
{
    // TODO: implement me!
    return u"";
}

void HTMLImageElementImp::setLongDesc(std::u16string longDesc)
{
    // TODO: implement me!
}

unsigned int HTMLImageElementImp::getVspace()
{
    // TODO: implement me!
    return 0;
}

void HTMLImageElementImp::setVspace(unsigned int vspace)
{
    // TODO: implement me!
}

}  // org::w3c::dom::bootstrap

namespace html {

namespace {

class Constructor : public Object
{
public:
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv) {
        bootstrap::HTMLImageElementImp* img = 0;
        switch (argc) {
        case 0:
            img = new(std::nothrow) bootstrap::HTMLImageElementImp(0);
            break;
        case 1:
            img = new(std::nothrow) bootstrap::HTMLImageElementImp(0);
            if (img)
                img->setWidth(argv[0]);
            break;
        case 2:
            img = new(std::nothrow) bootstrap::HTMLImageElementImp(0);
            if (img) {
                img->setWidth(argv[0]);
                img->setHeight(argv[1]);
            }
            break;
        default:
            break;
        }
        return img;
    }
    Constructor() :
        Object(this) {
    }
};

}  // namespace

Object HTMLImageElement::getConstructor()
{
    static Constructor constructor;
    return constructor.self();
}

}

}}}  // org::w3c::dom