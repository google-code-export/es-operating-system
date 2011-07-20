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

#include "Box.h"

#include <Object.h>
#include <org/w3c/dom/Document.h>
#include <org/w3c/dom/Element.h>
#include <org/w3c/dom/Text.h>
#include <org/w3c/dom/html/HTMLIFrameElement.h>
#include <org/w3c/dom/html/HTMLImageElement.h>
#include <org/w3c/dom/html/HTMLInputElement.h>

#include <algorithm>
#include <new>
#include <iostream>

#include "CSSSerialize.h"
#include "CSSStyleDeclarationImp.h"
#include "CSSTokenizer.h"
#include "ViewCSSImp.h"
#include "WindowImp.h"

namespace org { namespace w3c { namespace dom { namespace bootstrap {

namespace {

Element getContainingElement(Node node)
{
    for (; node; node = node.getParentNode()) {
        if (node.getNodeType() == Node::ELEMENT_NODE)
            return interface_cast<Element>(node);
    }
    return 0;
}

}

Box::Box(Node node) :
    node(node),
    parentBox(0),
    firstChild(0),
    lastChild(0),
    previousSibling(0),
    nextSibling(0),
    childCount(0),
    marginTop(0.0f),
    marginBottom(0.0f),
    marginLeft(0.0f),
    marginRight(0.0f),
    paddingTop(0.0f),
    paddingBottom(0.0f),
    paddingLeft(0.0f),
    paddingRight(0.0f),
    borderTop(0.0f),
    borderBottom(0.0f),
    borderLeft(0.0f),
    borderRight(0.0f),
    offsetH(0.0f),
    offsetV(0.0f),
    backgroundColor(0x00000000),
    backgroundImage(0),
    backgroundLeft(0.0f),
    backgroundTop(0.0f),
    style(0),
    formattingContext(0),
    flags(0),
    shadow(0)
{
}

Box::~Box()
{
    while (0 < childCount) {
        Box* child = removeChild(firstChild);
        child->release_();
    }
    delete backgroundImage;
}

Box* Box::removeChild(Box* item)
{
    Box* next = item->nextSibling;
    Box* prev = item->previousSibling;
    if (!next)
        lastChild = prev;
    else
        next->previousSibling = prev;
    if (!prev)
        firstChild = next;
    else
        prev->nextSibling = next;
    item->parentBox = 0;
    --childCount;
    return item;
}

Box* Box::insertBefore(Box* item, Box* after)
{
    if (!after)
        return appendChild(item);
    item->previousSibling = after->previousSibling;
    item->nextSibling = after;
    after->previousSibling = item;
    if (!item->previousSibling)
        firstChild = item;
    else
        item->previousSibling->nextSibling = item;
    item->parentBox = this;
    item->retain_();
    ++childCount;
    return item;
}

Box* Box::appendChild(Box* item)
{
    Box* prev = lastChild;
    if (!prev)
        firstChild = item;
    else
        prev->nextSibling = item;
    item->previousSibling = prev;
    item->nextSibling = 0;
    lastChild = item;
    item->parentBox = this;
    item->retain_();
    ++childCount;
    return item;
}

Box* Box::getParentBox() const
{
    return parentBox;
}

bool Box::hasChildBoxes() const
{
    return firstChild;
}

Box* Box::getFirstChild() const
{
    return firstChild;
}

Box* Box::getLastChild() const
{
    return lastChild;
}

Box* Box::getPreviousSibling() const
{
    return previousSibling;
}

Box* Box::getNextSibling() const
{
    return nextSibling;
}

void Box::updatePadding()
{
    paddingTop = style->paddingTop.getPx();
    paddingRight = style->paddingRight.getPx();
    paddingBottom = style->paddingBottom.getPx();
    paddingLeft = style->paddingLeft.getPx();
}

void Box::updateBorderWidth()
{
    borderTop = style->borderTopWidth.getPx();
    borderRight = style->borderRightWidth.getPx();
    borderBottom = style->borderBottomWidth.getPx();
    borderLeft = style->borderLeftWidth.getPx();
}

const ContainingBlock* Box::getContainingBlock(ViewCSSImp* view) const
{
    const Box* box = this;
    do {
        const Box* parent = box->getParentBox();
        if (!parent)
            return view->getInitialContainingBlock();
        box = parent;
    } while (box->getBoxType() != BLOCK_LEVEL_BOX);
    return box;
}

// We also calculate offsetH and offsetV here.
void BlockLevelBox::setContainingBlock(ViewCSSImp* view)
{
    assert(isAbsolutelyPositioned());
    float x = 0.0f;
    float y = 0.0f;
    Box::toViewPort(x, y);  // TODO: Why do we need to say 'Box::' here?
    offsetH = -x;
    offsetV = -y;
    if (!isFixed()) {
        assert(node);
        for (auto ancestor = node.getParentElement(); ancestor; ancestor = ancestor.getParentElement()) {
            CSSStyleDeclarationImp* style = view->getStyle(ancestor);
            if (!style)
                break;
            switch (style->position.getValue()) {
            case CSSPositionValueImp::Absolute:
            case CSSPositionValueImp::Relative:
            case CSSPositionValueImp::Fixed: {
                // Now we need to find the corresponding box for this ancestor.
                Box* box = style->box;
                assert(box);
                float t = -box->paddingTop;
                float l = -box->paddingLeft;
                box->toViewPort(l, t);
                if (box->getBoxType() == BLOCK_LEVEL_BOX) {
                    absoluteBlock.width = box->paddingLeft + box->width + box->paddingRight;
                    absoluteBlock.height = box->paddingTop + box->height + box->paddingBottom;
                } else {
                    assert(box->getBoxType() == INLINE_LEVEL_BOX);
                    float b = style->lastBox->height + style->lastBox->paddingBottom;
                    float r = style->lastBox->width + style->lastBox->paddingRight;
                    style->lastBox->toViewPort(r, b);
                    absoluteBlock.width = r - l;
                    absoluteBlock.height = t - b;
                }
                offsetH += l;
                offsetV += t;
                return;
            }
            default:
                break;
            }
        }
    }
    absoluteBlock.width = view->getInitialContainingBlock()->width;
    absoluteBlock.height = view->getInitialContainingBlock()->height;
}

FormattingContext* Box::updateFormattingContext(FormattingContext* context)
{
    if (isFlowRoot())
        return formattingContext;
    // TODO: adjust left and right blanks.
    return context;
}

bool Box::isFlowOf(const Box* flowRoot) const
{
    assert(flowRoot->isFlowRoot());
    for (const Box* box = this; box; box = box->getParentBox()) {
        if (box == flowRoot)
            return true;
    }
    return false;
}

// Calculate left, right, top, bottom for 'static' or 'relative' element.
// TODO: rtl
void Box::resolveOffset(ViewCSSImp* view)
{
    if (isAnonymous())
        return;

    assert(style->position.getValue() == CSSPositionValueImp::Relative ||
           style->position.getValue() == CSSPositionValueImp::Static);

    float h = 0.0f;
    if (!style->left.isAuto())
        h = style->left.getPx();
    else if (!style->right.isAuto())
        h = -style->right.getPx();
    offsetH += h;

    float v = 0.0f;
    if (!style->top.isAuto())
        v = style->top.getPx();
    else if (!style->bottom.isAuto())
        v = -style->bottom.getPx();
    offsetV += v;
}

bool BlockLevelBox::isFloat() const
{
    return style && style->isFloat();
}

bool BlockLevelBox::isFixed() const
{
    return style && style->position.getValue() == CSSPositionValueImp::Fixed;
}

bool BlockLevelBox::isAbsolutelyPositioned() const
{
    return style && style->isAbsolutelyPositioned();
}

void BlockLevelBox::dump(ViewCSSImp* view, std::string indent)
{
    std::cout << indent << "* block-level box [";
    if (node)
        std::cout << node.getNodeName();
    else
        std::cout << "anonymous";
    std::cout << "]\n";
    indent += "    ";
    if (!getFirstChild() && hasInline()) {
        for (auto i = inlines.begin(); i != inlines.end(); ++i) {
            if ((*i).getNodeType() == Node::TEXT_NODE) {
                Text text = interface_cast<Text>(*i);
                std::cout << indent << "* inline-level box: \"" << text.getData() << "\"\n";
            } else if (Box* box = view->getFloatBox(*i))
                box->dump(view, indent);
        }
        return;
    }
    for (Box* child = getFirstChild(); child; child = child->getNextSibling())
        child->dump(view, indent);
}

BlockLevelBox* BlockLevelBox::getAnonymousBox()
{
    BlockLevelBox* anonymousBox;
    if (firstChild && firstChild->isAnonymous()) {
        anonymousBox = dynamic_cast<BlockLevelBox*>(firstChild);
        if (anonymousBox)
            return anonymousBox;
    }
    anonymousBox = new(std::nothrow) BlockLevelBox;
    if (anonymousBox) {
        anonymousBox->spliceInline(this);
        if (firstChild)
            insertBefore(anonymousBox, firstChild);
        else
            appendChild(anonymousBox);
    }
    return anonymousBox;
}

void BlockLevelBox::resolveWidth(ViewCSSImp* view, const ContainingBlock* containingBlock, float available)
{
    assert(style);
    backgroundColor = style->backgroundColor.getARGB();
    if (!style->backgroundImage.isNone())
        backgroundImage = new(std::nothrow) BoxImage(this, view->getDocument().getDocumentURI(), style->backgroundImage.getValue(), style->backgroundRepeat.getValue());

    //
    // Calculate width
    //
    // marginLeft + borderLeftWidth + paddingLeft + width + paddingRight + borderRightWidth + marginRight
    // == containingBlock->width (- scrollbar width, if any)
    //
    updatePadding();
    updateBorderWidth();

    int autoCount = 3;  // properties which can be auto are margins and width
    if (!style->marginLeft.isAuto()) {
        marginLeft = style->marginLeft.getPx();
        --autoCount;
    }
    if (!style->marginRight.isAuto()) {
        marginRight = style->marginRight.getPx();
        --autoCount;
    }
    if (!style->width.isAuto()) {
        width = style->width.getPx();
        --autoCount;
    } else if (available) {
        width = available;
        --autoCount;
    }
    // if 0 == autoCount, the values are over-constrained; ignore marginRight if 'ltr'
    float leftover = containingBlock->width - paddingLeft - paddingRight - borderLeft - borderRight - marginLeft - marginRight - width;
    // if leftover < 0, follow overflow
    if (autoCount == 1) {
        if (style->marginLeft.isAuto())
            marginLeft = leftover;
        else if (style->marginRight.isAuto())
            marginRight = leftover;
        else
            width = leftover;
    } else if (style->width.isAuto()) {
        if (style->marginLeft.isAuto())
            marginLeft = 0.0f;
        if (style->marginRight.isAuto())
            marginRight = 0.0f;
        width = leftover;
    } else if (style->marginLeft.isAuto() && style->marginRight.isAuto())
        marginLeft = marginRight = leftover / 2.0f;

    if (!style->marginTop.isAuto())
        marginTop = style->marginTop.getPx();
    else
        marginTop = 0;
    if (!style->marginBottom.isAuto())
        marginBottom = style->marginBottom.getPx();
    else
        marginBottom = 0;

    if (!style->height.isAuto())
        height = style->height.getPx();
}

void BlockLevelBox::layOutText(ViewCSSImp* view, Text text, FormattingContext* context)
{
    CSSStyleDeclarationImp* style = 0;
    Element element = getContainingElement(text);
    if (!element)
        return;  // TODO error

    style = view->getStyle(element);
    if (!style)
        return;  // TODO error
    style->resolve(view, this, element);

    std::u16string data = text.getData();
    if (style->processWhiteSpace(data, context->prevChar) == 0)
        return;

    bool psuedoChecked = false;
    CSSStyleDeclarationImp* firstLineStyle = 0;
    CSSStyleDeclarationImp* firstLetterStyle = 0;
    CSSStyleDeclarationImp* activeStyle = style;

    FontTexture* font = view->selectFont(activeStyle);
    float point = view->getPointFromPx(activeStyle->fontSize.getPx());
    size_t position = 0;
    for (;;) {
        if (!context->lineBox) {
            if (style->processLineHeadWhiteSpace(data) == 0)
                return;
            if (!context->addLineBox(view, this))
                return;  // TODO error
            if (!psuedoChecked && getFirstChild() == context->lineBox) {
                psuedoChecked  = true;
                // The current line box is the 1st line of this block box.
                // style to use can be a pseudo element styles from any ancestor elements.
                // Note the :first-line, first-letter pseudo-elements can only be attached to a block container element.
                std::list<CSSStyleDeclarationImp*> firstLineStyles;
                std::list<CSSStyleDeclarationImp*> firstLetterStyles;
                Box* box = this;
                for (;;) {
                    if (CSSStyleDeclarationImp* s = box->getStyle()) {
                        if (CSSStyleDeclarationImp* p = s->getPseudoElementStyle(CSSStyleDeclarationImp::FirstLine))
                            firstLineStyles.push_front(p);
                        if (CSSStyleDeclarationImp* p = s->getPseudoElementStyle(CSSStyleDeclarationImp::FirstLetter))
                            firstLetterStyles.push_front(p);
                    }
                    Box* parent = box->getParentBox();
                    if (!parent || parent->getFirstChild() != box)
                        break;
                    box = parent;
                }
                if (!firstLineStyles.empty()) {
                    CSSStyleDeclarationImp* parentStyle = style;
                    for (auto i = firstLineStyles.begin(); i != firstLineStyles.end(); ++i) {
                        (*i)->compute(view, parentStyle, 0);
                        parentStyle = *i;
                    }
                    firstLineStyle = firstLineStyles.back();
                    CSSStyleDeclarationImp* p = style->createPseudoElementStyle(CSSStyleDeclarationImp::FirstLine);
                    assert(p);  // TODO
                    p->specify(firstLineStyle);
                    firstLineStyle = p;
                }
                if (!firstLetterStyles.empty()) {
                    CSSStyleDeclarationImp* parentStyle = style;
                    for (auto i = firstLetterStyles.begin(); i != firstLetterStyles.end(); ++i) {
                        (*i)->compute(view, parentStyle, 0);
                        parentStyle = *i;
                    }
                    firstLetterStyle = firstLetterStyles.back();
                    CSSStyleDeclarationImp* p = style->createPseudoElementStyle(CSSStyleDeclarationImp::FirstLetter);
                    assert(p);  // TODO
                    p->specify(firstLetterStyle);
                    firstLetterStyle = p;
                }
                if (firstLetterStyle) {
                    activeStyle = firstLetterStyle;
                    font = view->selectFont(activeStyle);
                    point = view->getPointFromPx(activeStyle->fontSize.getPx());
                } else if (firstLineStyle) {
                    activeStyle = firstLineStyle;
                    font = view->selectFont(activeStyle);
                    point = view->getPointFromPx(activeStyle->fontSize.getPx());
                }
            }
        }

        InlineLevelBox* inlineLevelBox = new(std::nothrow) InlineLevelBox(text, activeStyle);
        if (!inlineLevelBox)
            return;  // TODO error
        style->addBox(inlineLevelBox);  // activeStyle? maybe not...
        inlineLevelBox->resolveWidth();
        float blankLeft = inlineLevelBox->getBlankLeft();
        if (0 < position)  // TODO: this is a bit awkward...
            inlineLevelBox->marginLeft = inlineLevelBox->paddingLeft = inlineLevelBox->borderLeft = blankLeft = 0;
        float blankRight = inlineLevelBox->getBlankRight();
        context->x += blankLeft;
        context->leftover -= blankLeft + blankRight;

        size_t fitLength = firstLetterStyle ? 1 : data.length();  // TODO: 1 is absolutely wrong...

        // We are still not sure if there's a room for text in context->lineBox.
        // If there's no room due to float box(es), move the linebox down to
        // the closest bottom of float box.
        // And repeat this process until there's no more float box in the context.
        float advanced;
        size_t length;
        do {
            advanced = context->leftover;
            length = font->fitText(data.c_str(), fitLength, point, context->leftover);
            if (0 < length) {
                advanced -= context->leftover;
                break;
            }
        } while (context->shiftDownLineBox());
        // TODO: deal with overflow

        inlineLevelBox->setData(font, point, data.substr(0, length));
        inlineLevelBox->width = advanced;
        if (!firstLetterStyle && length < data.length()) {  // TODO: firstLetterStyle : actually we are not sure if the following characters would fit in the same line box...
            inlineLevelBox->marginRight = inlineLevelBox->paddingRight = inlineLevelBox->borderRight = blankRight = 0;
        }
        inlineLevelBox->height = font->getHeight(point);
        inlineLevelBox->baseline += (activeStyle->lineHeight.getPx() - inlineLevelBox->height) / 2.0f;
        context->x += advanced + blankRight;
        context->lineBox->appendChild(inlineLevelBox);
        context->lineBox->height = std::max(context->lineBox->height, std::max(activeStyle->lineHeight.getPx(), inlineLevelBox->height));
        context->lineBox->baseline = std::max(context->lineBox->baseline, inlineLevelBox->baseline);
        context->lineBox->width += blankLeft + advanced + blankRight;
        position += length;
        data.erase(0, length);
        if (data.length() == 0)  // layout done?
            break;

        if (firstLetterStyle) {
            firstLetterStyle = 0;
            if (firstLineStyle) {
                activeStyle = firstLineStyle;
                font = view->selectFont(activeStyle);
                point = view->getPointFromPx(activeStyle->fontSize.getPx());
            } else {
                activeStyle = style;
                font = view->selectFont(activeStyle);
                point = view->getPointFromPx(activeStyle->fontSize.getPx());
            }
        } else {
            context->nextLine(this);
            if (firstLineStyle) {
                firstLineStyle = 0;
                activeStyle = style;
                font = view->selectFont(activeStyle);
                point = view->getPointFromPx(activeStyle->fontSize.getPx());
            }
        }
    }
}

void BlockLevelBox::layOutInlineReplaced(ViewCSSImp* view, Node node, FormattingContext* context)
{
    CSSStyleDeclarationImp* style = 0;
    Element element = 0;
    if (element = getContainingElement(node)) {
        style = view->getStyle(element);
        if (!style)
            return;  // TODO error
        style->resolve(view, this, element);
    } else
        return;  // TODO error

    if (!context->lineBox) {
        if (!context->addLineBox(view, this))
            return;  // TODO error
    }
    InlineLevelBox* inlineLevelBox = new(std::nothrow) InlineLevelBox(node, style);
    if (!inlineLevelBox)
        return;  // TODO error
    style->addBox(inlineLevelBox);

    inlineLevelBox->resolveWidth();
    if (!style->width.isAuto())
        inlineLevelBox->width = style->width.getPx();
    else
        inlineLevelBox->width = 300;  // TODO
    if (!style->height.isAuto())
        inlineLevelBox->height = style->height.getPx();
    else
        inlineLevelBox->height = 24;  // TODO

    context->prevChar = 0;

    std::u16string tag = element.getLocalName();
    if (tag == u"img") {
        html::HTMLImageElement img = interface_cast<html::HTMLImageElement>(element);
        if (BoxImage* backgroundImage = new(std::nothrow) BoxImage(inlineLevelBox, view->getDocument().getDocumentURI(), img)) {
            inlineLevelBox->backgroundImage = backgroundImage;
            inlineLevelBox->width = backgroundImage->getWidth();
            inlineLevelBox->height = backgroundImage->getHeight();
        }
    } else if (tag == u"iframe") {
        html::HTMLIFrameElement iframe = interface_cast<html::HTMLIFrameElement>(element);
        inlineLevelBox->width = CSSTokenizer::parseInt(iframe.getWidth().c_str(), iframe.getWidth().size());
        inlineLevelBox->height = CSSTokenizer::parseInt(iframe.getHeight().c_str(), iframe.getHeight().size());
        html::Window contentWindow = iframe.getContentWindow();
        if (WindowImp* imp = dynamic_cast<WindowImp*>(contentWindow.self())) {
            imp->setSize(inlineLevelBox->width, inlineLevelBox->height);
            inlineLevelBox->shadow = imp->getView();
            std::u16string src = iframe.getSrc();
            if (!src.empty() && !imp->getDocument())
                iframe.setSrc(src);
        }
    } else if (tag == u"input") {
        // TODO: assume text for now...
        html::HTMLInputElement input = interface_cast<html::HTMLInputElement>(element);
        Element div = view->getDocument().createElement(u"div");
        Text text = view->getDocument().createTextNode(input.getValue());
        if (div && text) {
            div.appendChild(text);

            // Create the shadow DOM style on the fly (TODO: at least for now...)
            CSSParser parser;
            parser.parseDeclarations(u"display: block; border-style: solid; border-width: thin;");
            CSSStyleDeclarationImp* divStyle = parser.getStyleDeclaration();
            divStyle->compute(view, style, div);
            divStyle->height.setValue(style->lineHeight.getPx(), css::CSSPrimitiveValue::CSS_PX);
            view->map[div] = divStyle;

            if (BlockLevelBox* box = view->layOutBlockBoxes(div, 0, 0, 0)) {
                inlineLevelBox->appendChild(box);
                box->layOut(view, context);
            }
        }
    }

    // TODO: calc inlineLevelBox->width and height with intrinsic values.
    inlineLevelBox->baseline = inlineLevelBox->height;  // TODO

    float blankLeft = inlineLevelBox->getBlankLeft();
    float blankRight = inlineLevelBox->getBlankRight();
    context->x += blankLeft;
    context->leftover -= blankLeft + blankRight;

    // We are still not sure if there's a room for text in context->lineBox.
    // If there's no room due to float box(es), move the linebox down to
    // the closest bottom of float box.
    // And repeat this process until there's no more float box in the context.
    do {
        if (inlineLevelBox->width <= context->leftover) {
            context->leftover -= inlineLevelBox->width;
            break;
        }
    } while (context->shiftDownLineBox());
    // TODO: deal with overflow

    context->x += inlineLevelBox->width + blankRight;
    context->lineBox->appendChild(inlineLevelBox);
    context->lineBox->height = std::max(context->lineBox->height, inlineLevelBox->height);  // TODO: marginTop, etc.???
    context->lineBox->baseline = std::max(context->lineBox->baseline, inlineLevelBox->baseline);
    context->lineBox->width += blankLeft + inlineLevelBox->width + blankRight;
}

void BlockLevelBox::layOutFloat(ViewCSSImp* view, Node node, BlockLevelBox* floatBox, FormattingContext* context)
{
    assert(floatBox->style);
    floatBox->layOut(view, context);
    floatBox->remainingHeight = floatBox->getTotalHeight();
    float w = floatBox->getTotalWidth();
    if (context->leftover < w) {  // TODO && there's at least one float
        // Process this float box later in the other line box.
        context->floatNodes.push_back(node);
        return;
    }
    context->addFloat(floatBox, w);
}

void BlockLevelBox::layOutAbsolute(ViewCSSImp* view, Node node, BlockLevelBox* absBox, FormattingContext* context)
{
    // Just insert this absolute box into a line box. We will the abs. box later. TODO
    if (!context->lineBox) {
        if (!context->addLineBox(view, this))
            return;  // TODO error
    }
    context->lineBox->appendChild(absBox);
}

// Generate line boxes
void BlockLevelBox::layOutInline(ViewCSSImp* view, FormattingContext* context)
{
    assert(!hasChildBoxes());
    for (auto i = inlines.begin(); i != inlines.end(); ++i) {
        if ((*i).getNodeType() == Node::TEXT_NODE) {
            Text text = interface_cast<Text>(*i);
            layOutText(view, text, context);
        } else if (BlockLevelBox* box = view->getFloatBox(*i)) {
            if (box->isFloat())
                layOutFloat(view, *i, box, context);
            else if (box->isAbsolutelyPositioned())
                layOutAbsolute(view, *i, box, context);
        } else {
            // TODO: At this point, *i should be a replaced element, but it could be of other types as we develop...
            layOutInlineReplaced(view, *i, context);
        }
    }
    if (context->lineBox)
        context->nextLine(this);
    // Layout remaining float boxes in context
    while (!context->floatNodes.empty()) {
        context->addLineBox(view, this);
        context->nextLine(this);
    }
}

void BlockLevelBox::layOut(ViewCSSImp* view, FormattingContext* context)
{
    const ContainingBlock* containingBlock = getContainingBlock(view);

    Element element = 0;
    if (!isAnonymous())
        element = getContainingElement(node);
    else if (const Box* box = dynamic_cast<const Box*>(containingBlock))
        element = getContainingElement(box->node);
    if (!element)
        return;  // TODO error

    style = view->getStyle(element);
    if (!style)
        return;  // TODO error

    if (!isAnonymous()) {
        style->resolve(view, containingBlock, element);
        resolveWidth(view, containingBlock, style->isFloat() ? context->leftover : 0.0f);
    } else {
        // The properties of anonymous boxes are inherited from the enclosing non-anonymous box.
        // Theoretically, we are supposed to create a new style for this anonymous box, but
        // of course we don't want to do so.
        backgroundColor = 0x00000000;
        paddingTop = paddingRight = paddingBottom = paddingLeft = 0.0f;
        borderTop = borderRight = borderBottom = borderLeft = 0.0f;
        marginTop = marginRight = marginLeft = marginBottom = 0.0f;
        width = containingBlock->width;
        height = containingBlock->height;
    }

    textAlign = style->textAlign.getValue();

    // Collapse margins
    if (Box* parent = getParentBox()) {  // TODO: root exception
        if (parent->getFirstChild() == this &&
            parent->borderTop == 0 && parent->paddingTop == 0 /* && TODO: check clearance */) {
            marginTop = std::max(marginTop, parent->marginTop);  // TODO: negative case
            parent->marginTop = 0.0f;
        } else if (Box* prev = getPreviousSibling()) {
            marginTop = std::max(prev->marginBottom, marginTop);  // TODO: negative case
            prev->marginBottom = 0.0f;
        }
    }

    context = updateFormattingContext(context);
    assert(context);
    context->updateRemainingHeight(getBlankTop());
    context->clear(this, style->clear.getValue());

    if (hasInline())
        layOutInline(view, context);
    for (Box* child = getFirstChild(); child; child = child->getNextSibling())
        child->layOut(view, context);

    // Collapse marginBottom  // TODO: root exception
    if (height == 0 && borderBottom == 0 && paddingBottom == 0 /* && TODO: check clearance */) {
        if (Box* child = getLastChild()) {
            marginBottom = std::max(marginBottom, child->marginBottom);  // TODO: negative case
            child->marginBottom = 0;
        }
    }

    // simplified shrink-to-fit
    if (style->isFloat() && style->width.isAuto()) {
        width = 0.0f;
        for (Box* child = getFirstChild(); child; child = child->getNextSibling())
            width = std::max(width, child->getTotalWidth());
    }

    if (height == 0) {
        for (Box* child = getFirstChild(); child; child = child->getNextSibling())
            height += child->getTotalHeight();
    }

    // Now that 'height' is fixed, calculate 'left', 'right', 'top', and 'bottom'.
    for (Box* child = getFirstChild(); child; child = child->getNextSibling())
        child->resolveOffset(view);

    if (backgroundImage && backgroundImage->getState() == BoxImage::CompletelyAvailable) {
        style->backgroundPosition.compute(view, backgroundImage, style->fontSize, getPaddingWidth(), getPaddingHeight());
        backgroundLeft = style->backgroundPosition.getLeftPx();
        backgroundTop = style->backgroundPosition.getTopPx();
    }
}

void BlockLevelBox::resolveAbsoluteWidth(const ContainingBlock* containingBlock)
{
    assert(style);
    backgroundColor = style->backgroundColor.getARGB();

    updatePadding();
    updateBorderWidth();

    //
    // Calculate width
    //
    // left + marginLeft + borderLeftWidth + paddingLeft + width + paddingRight + borderRightWidth + marginRight + right
    // == containingBlock->width
    //
    enum {
        Left = 4u,
        Width = 2u,
        Right = 1u
    };

    marginLeft = style->marginLeft.isAuto() ? 0.0f : style->marginLeft.getPx();
    marginRight = style->marginRight.isAuto() ? 0.0f : style->marginRight.getPx();

    float left = 0.0f;
    float right = 0.0f;

    unsigned autoMask = Left | Width | Right;
    if (!style->left.isAuto()) {
        left = style->left.getPx();
        autoMask &= ~Left;
    }
    if (!style->width.isAuto()) {
        width = style->width.getPx();
        autoMask &= ~Width;
    }
    if (!style->right.isAuto()) {
        right = style->right.getPx();
        autoMask &= ~Right;
    }
    float leftover = containingBlock->width - getTotalWidth() - left - right;
    switch (autoMask) {
    case Left | Width | Right:
        left = -offsetH;
        autoMask &= ~Left;
        // FALL THROUGH
    case Width | Right:
        // TODO shrink-to-fit
        width += leftover;
        break;
    case Left | Width:
        // TODO shrink-to-fit
        width += leftover;
        break;
    case Left | Right:
        left = -offsetH;
        right += leftover - left;
        break;
    case Left:
        left += leftover;
        break;
    case Width:
        width += leftover;
        break;
    case Right:
        right += leftover;
        break;
    case 0:
        if (style->marginLeft.isAuto() && style->marginRight.isAuto()) {
            if (0.0f <= leftover)
                marginLeft = marginRight = leftover / 2.0f;
            else {  // TODO rtl
                marginLeft = 0.0f;
                marginRight = -leftover;
            }
        } else if (style->marginLeft.isAuto())
            marginLeft = leftover;
        else if (style->marginRight.isAuto())
            marginRight = leftover;
        else
            right += leftover;
        break;
    }

    //
    // Calculate height
    //
    // top + marginTop + borderTopWidth + paddingTop + height + paddingBottom + borderBottomWidth + marginBottom + bottom
    // == containingBlock->height
    //
    enum {
        Top = 4u,
        Height = 2u,
        Bottom = 1u
    };

    marginTop = style->marginTop.isAuto() ? 0.0f : style->marginTop.getPx();
    marginBottom = style->marginBottom.isAuto() ? 0.0f : style->marginBottom.getPx();

    float top = 0.0f;
    float bottom = 0.0f;

    autoMask = Top | Height | Bottom;
    if (!style->top.isAuto()) {
        top = style->top.getPx();
        autoMask &= ~Top;
    }
    if (!style->height.isAuto()) {
        height = style->height.getPx();
        autoMask &= ~Height;
    }
    if (!style->bottom.isAuto()) {
        bottom = style->bottom.getPx();
        autoMask &= ~Bottom;
    }
    leftover = containingBlock->height - getTotalHeight() - top - bottom;
    switch (autoMask) {
    case Top | Height | Bottom:
        top = -offsetV;
        autoMask &= ~Top;
        // FALL THROUGH
    case Height | Bottom:
        // TODO shrink-to-fit
        height += leftover;
        break;
    case Top | Height:
        // TODO shrink-to-fit
        height += leftover;
        break;
    case Top | Bottom:
        top = -offsetV;
        bottom += leftover - top;
        break;
    case Top:
        top += leftover;
        break;
    case Height:
        height += leftover;
        break;
    case Bottom:
        bottom += leftover;
        break;
    case 0:
        if (style->marginTop.isAuto() && style->marginBottom.isAuto()) {
            if (0.0f <= leftover)
                marginTop = marginBottom = leftover / 2.0f;
            else {  // TODO rtl
                marginTop = 0.0f;
                marginBottom = -leftover;
            }
        } else if (style->marginTop.isAuto())
            marginTop = leftover;
        else if (style->marginBottom.isAuto())
            marginBottom = leftover;
        else
            bottom += leftover;
        break;
    }

    offsetH += left;
    offsetV += top;
}

void BlockLevelBox::layOutAbsolute(ViewCSSImp* view, Node node)
{
    assert(isAbsolutelyPositioned());
    setContainingBlock(view);
    const ContainingBlock* containingBlock = &absoluteBlock;

    Element element = getContainingElement(node);
    if (!element)
        return;  // TODO error
    if (!style)
        return;  // TODO error

    style->resolve(view, containingBlock, element);
    resolveAbsoluteWidth(containingBlock);

    FormattingContext* context = updateFormattingContext(context);
    assert(context);

    if (hasInline())
        layOutInline(view, context);
    for (Box* child = getFirstChild(); child; child = child->getNextSibling())
        child->layOut(view, context);

    if (height == 0) {
        for (Box* child = getFirstChild(); child; child = child->getNextSibling())
            height += child->marginTop + child->borderTop + child->paddingTop +
                      child->height +
                      child->marginBottom + child->borderBottom + child->paddingBottom;
    }

    // Now that 'height' is fixed, calculate 'left', 'right', 'top', and 'bottom'.
    for (Box* child = getFirstChild(); child; child = child->getNextSibling())
        child->resolveOffset(view);
}

void LineBox::layOut(ViewCSSImp* view, FormattingContext* context)
{
    // Process 'vertical-align'
    // TODO: assume baseline for now
    for (Box* box = getFirstChild(); box; box = box->getNextSibling()) {
        if (box->isAbsolutelyPositioned())
            continue;
        box->resolveOffset(view);
        if (InlineLevelBox* inlineBox = dynamic_cast<InlineLevelBox*>(box))
            inlineBox->marginTop += getBaseline() - inlineBox->getBaseline();
    }
}

void LineBox::dump(ViewCSSImp* view, std::string indent)
{
    std::cout << indent << "* line box:\n";
    indent += "    ";
    for (Box* child = getFirstChild(); child; child = child->getNextSibling())
        child->dump(view, indent);
}

void InlineLevelBox::toViewPort(const Box* box, float& x, float& y) const
{
    // error
    assert(0);
}

bool InlineLevelBox::isAnonymous() const
{
    return !style || !style->display.isInlineLevel();
}

void InlineLevelBox::setData(FontTexture* font, float point, std::u16string data) {
    this->font = font;
    this->point = point;
    this->data = data;
    baseline = font->getAscender(point);
}

void InlineLevelBox::resolveWidth()
{
    // The ‘width’ and ‘height’ properties do not apply.
    if (!isAnonymous()) {
        backgroundColor = style->backgroundColor.getARGB();

        updatePadding();
        updateBorderWidth();

        marginTop = style->marginTop.isAuto() ? 0 : style->marginTop.getPx();
        marginRight = style->marginRight.isAuto() ? 0 : style->marginRight.getPx();
        marginLeft = style->marginLeft.isAuto() ? 0 : style->marginLeft.getPx();
        marginBottom = style->marginBottom.isAuto() ? 0 : style->marginBottom.getPx();
    } else {
        backgroundColor = 0x00000000;
        paddingTop = paddingRight = paddingBottom = paddingLeft = 0.0f;
        borderTop = borderRight = borderBottom = borderLeft = 0.0f;
        marginTop = marginRight = marginLeft = marginBottom = 0.0f;
    }
}

// Note inline-level boxes inherit their ancestors' offsets.
void InlineLevelBox::resolveOffset(ViewCSSImp* view)
{
    CSSStyleDeclarationImp* s = getStyle();
    Element element = getContainingElement(node);
    while (s && s->display.isInlineLevel()) {
        Box::resolveOffset(view);
        element = element.getParentElement();
        if (!element)
            break;
        s = view->getStyle(element);
    }
}

void InlineLevelBox::dump(ViewCSSImp* view, std::string indent)
{
    std::cout << indent << "* inline-level box: \"" << data << "\"\n";
    indent += "    ";
    for (Box* child = getFirstChild(); child; child = child->getNextSibling())
        child->dump(view, indent);
}

}}}}  // org::w3c::dom::bootstrap