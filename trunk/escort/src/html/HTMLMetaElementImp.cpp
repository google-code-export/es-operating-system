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

#include "HTMLMetaElementImp.h"

namespace org { namespace w3c { namespace dom { namespace bootstrap {

// Node
Node HTMLMetaElementImp::cloneNode(bool deep)
{
    return new(std::nothrow) HTMLMetaElementImp(this, deep);
}

// HTMLMetaElement
std::u16string HTMLMetaElementImp::getName()
{
    // TODO: implement me!
    return u"";
}

void HTMLMetaElementImp::setName(const std::u16string& name)
{
    // TODO: implement me!
}

std::u16string HTMLMetaElementImp::getHttpEquiv()
{
    // TODO: implement me!
    return u"";
}

void HTMLMetaElementImp::setHttpEquiv(const std::u16string& httpEquiv)
{
    // TODO: implement me!
}

std::u16string HTMLMetaElementImp::getContent()
{
    // TODO: implement me!
    return u"";
}

void HTMLMetaElementImp::setContent(const std::u16string& content)
{
    // TODO: implement me!
}

std::u16string HTMLMetaElementImp::getScheme()
{
    // TODO: implement me!
    return u"";
}

void HTMLMetaElementImp::setScheme(const std::u16string& scheme)
{
    // TODO: implement me!
}

}}}}  // org::w3c::dom::bootstrap
