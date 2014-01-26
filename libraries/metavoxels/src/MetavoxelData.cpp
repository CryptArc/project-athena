//
//  MetavoxelData.cpp
//  metavoxels
//
//  Created by Andrzej Kapolka on 12/6/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#include <QScriptEngine>
#include <QtDebug>

#include "MetavoxelData.h"

MetavoxelData::MetavoxelData() {
}

MetavoxelData::MetavoxelData(const MetavoxelData& other) : _roots(other._roots) {
    incrementRootReferenceCounts();
}

MetavoxelData::~MetavoxelData() {
    decrementRootReferenceCounts();
}

MetavoxelData& MetavoxelData::operator=(const MetavoxelData& other) {
    decrementRootReferenceCounts();
    _roots = other._roots;
    incrementRootReferenceCounts();
    return *this;
}

void MetavoxelData::guide(MetavoxelVisitor& visitor) {
    // start with the root values/defaults (plus the guide attribute)
    const float TOP_LEVEL_SIZE = 1.0f;
    const QVector<AttributePointer>& inputs = visitor.getInputs();
    const QVector<AttributePointer>& outputs = visitor.getOutputs();
    MetavoxelVisitation firstVisitation = { this, NULL, visitor, QVector<MetavoxelNode*>(inputs.size() + 1),
        QVector<MetavoxelNode*>(outputs.size()), { glm::vec3(), TOP_LEVEL_SIZE,
            QVector<AttributeValue>(inputs.size() + 1), QVector<AttributeValue>(outputs.size()) } };
    for (int i = 0; i < inputs.size(); i++) {
        MetavoxelNode* node = _roots.value(inputs.at(i));
        firstVisitation.inputNodes[i] = node;
        firstVisitation.info.inputValues[i] = node ? node->getAttributeValue(inputs[i]) : inputs[i];
    }
    AttributePointer guideAttribute = AttributeRegistry::getInstance()->getGuideAttribute();
    MetavoxelNode* node = _roots.value(guideAttribute);
    firstVisitation.inputNodes.last() = node;
    firstVisitation.info.inputValues.last() = node ? node->getAttributeValue(guideAttribute) : guideAttribute;
    for (int i = 0; i < outputs.size(); i++) {
        MetavoxelNode* node = _roots.value(outputs.at(i));
        firstVisitation.outputNodes[i] = node;
    }
    static_cast<MetavoxelGuide*>(firstVisitation.info.inputValues.last().getInlineValue<
        PolymorphicDataPointer>().data())->guide(firstVisitation);
    for (int i = 0; i < outputs.size(); i++) {
        AttributeValue& value = firstVisitation.info.outputValues[i];
        if (value.getAttribute()) {
            MetavoxelNode* node = firstVisitation.outputNodes.at(i);
            if (node->isLeaf() && value.isDefault()) {
                node->decrementReferenceCount(value.getAttribute());
                _roots.remove(value.getAttribute());
            }
        }
    }
}

void MetavoxelData::read(Bitstream& in) {
    // save the old roots and clear
    QHash<AttributePointer, MetavoxelNode*> oldRoots = _roots;
    _roots.clear();
    
    // read in the new roots, reusing old ones where appropriate
    int rootCount;
    in >> rootCount;
    for (int i = 0; i < rootCount; i++) {
        AttributePointer attribute;
        in.getAttributeStreamer() >> attribute;
        MetavoxelNode* root = oldRoots.take(attribute);
        if (!root) {
            root = new MetavoxelNode(attribute);
        }
        _roots.insert(attribute, root);
        root->read(attribute, in);
    }
    
    // clear out the remaining old roots
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = oldRoots.constBegin(); it != oldRoots.constEnd(); it++) {
        it.value()->decrementReferenceCount(it.key());
    }
}

void MetavoxelData::write(Bitstream& out) const {
    out << _roots.size();
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = _roots.constBegin(); it != _roots.constEnd(); it++) {
        out.getAttributeStreamer() << it.key();
        it.value()->write(it.key(), out);
    }
}

void MetavoxelData::readDelta(const MetavoxelData& reference, Bitstream& in) {
    int changedCount;
    in >> changedCount;
    for (int i = 0; i < changedCount; i++) {
        AttributePointer attribute;
        in.getAttributeStreamer() >> attribute;
        MetavoxelNode*& root = _roots[attribute];
        if (!root) {
            root = new MetavoxelNode(attribute);
        }
        MetavoxelNode* referenceRoot = reference._roots.value(attribute);
        if (referenceRoot) {
            root->readDelta(attribute, *referenceRoot, in);    
                
        } else {
            root->read(attribute, in);
        }
    }
    
    int removedCount;
    in >> removedCount;
    for (int i = 0; i < removedCount; i++) {
        AttributePointer attribute;
        in.getAttributeStreamer() >> attribute;
        
    }
}

void MetavoxelData::writeDelta(const MetavoxelData& reference, Bitstream& out) const {
    // count the number of roots added/changed, then write
    int changedCount = 0;
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = _roots.constBegin(); it != _roots.constEnd(); it++) {
        MetavoxelNode* referenceRoot = reference._roots.value(it.key());
        if (it.value() != referenceRoot) {
            changedCount++;
        }
    }
    out << changedCount;
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = _roots.constBegin(); it != _roots.constEnd(); it++) {
        MetavoxelNode* referenceRoot = reference._roots.value(it.key());
        if (it.value() != referenceRoot) {
            out.getAttributeStreamer() << it.key();
            if (referenceRoot) {
                it.value()->writeDelta(it.key(), *referenceRoot, out);
            } else {
                it.value()->write(it.key(), out);
            }
        }
    }
    
    // same with nodes removed
    int removedCount = 0;
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = reference._roots.constBegin();
            it != reference._roots.constEnd(); it++) {
        if (!_roots.contains(it.key())) {
            removedCount++;
        }
    }
    out << removedCount;
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = reference._roots.constBegin();
            it != reference._roots.constEnd(); it++) {
        if (!_roots.contains(it.key())) {
            out.getAttributeStreamer() << it.key();
        }
    }
}

void MetavoxelData::incrementRootReferenceCounts() {
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = _roots.constBegin(); it != _roots.constEnd(); it++) {
        it.value()->incrementReferenceCount();
    }
}

void MetavoxelData::decrementRootReferenceCounts() {
    for (QHash<AttributePointer, MetavoxelNode*>::const_iterator it = _roots.constBegin(); it != _roots.constEnd(); it++) {
        it.value()->decrementReferenceCount(it.key());
    }
}

MetavoxelNode::MetavoxelNode(const AttributeValue& attributeValue) : _referenceCount(1) {
    _attributeValue = attributeValue.copy();
    for (int i = 0; i < CHILD_COUNT; i++) {
        _children[i] = NULL;
    }
}

void MetavoxelNode::setAttributeValue(const AttributeValue& attributeValue) {
    attributeValue.getAttribute()->destroy(_attributeValue);
    _attributeValue = attributeValue.copy();
    clearChildren(attributeValue.getAttribute());
}

AttributeValue MetavoxelNode::getAttributeValue(const AttributePointer& attribute) const {
    return AttributeValue(attribute, _attributeValue);
}

void MetavoxelNode::mergeChildren(const AttributePointer& attribute) {
    void* childValues[CHILD_COUNT];
    bool allLeaves = true;
    for (int i = 0; i < CHILD_COUNT; i++) {
        childValues[i] = _children[i]->_attributeValue;
        allLeaves &= _children[i]->isLeaf();
    }
    if (attribute->merge(_attributeValue, childValues) && allLeaves) {
        clearChildren(attribute);
    }
}

bool MetavoxelNode::isLeaf() const {
    for (int i = 0; i < CHILD_COUNT; i++) {
        if (_children[i]) {
            return false;
        }    
    }
    return true;
}

void MetavoxelNode::read(const AttributePointer& attribute, Bitstream& in) {
    bool leaf;
    in >> leaf;
    attribute->read(in, _attributeValue, leaf);
    if (leaf) {
        clearChildren(attribute);
        
    } else {
        void* childValues[CHILD_COUNT];
        for (int i = 0; i < CHILD_COUNT; i++) {
            if (!_children[i]) {
                _children[i] = new MetavoxelNode(attribute);
            }
            _children[i]->read(attribute, in);
            childValues[i] = _children[i]->_attributeValue;
        }
        attribute->merge(_attributeValue, childValues);
    }
}

void MetavoxelNode::write(const AttributePointer& attribute, Bitstream& out) const {
    bool leaf = isLeaf();
    out << leaf;
    attribute->write(out, _attributeValue, leaf);
    if (!leaf) {
        for (int i = 0; i < CHILD_COUNT; i++) {
            _children[i]->write(attribute, out);
        }
    }
}

void MetavoxelNode::readDelta(const AttributePointer& attribute, const MetavoxelNode& reference, Bitstream& in) {
    bool leaf;
    in >> leaf;
    attribute->readDelta(in, _attributeValue, reference._attributeValue, leaf);
    if (leaf) {
        clearChildren(attribute);
        
    } else {
        if (reference.isLeaf()) {
            for (int i = 0; i < CHILD_COUNT; i++) {
                _children[i]->read(attribute, in);
            }
        } else {
            for (int i = 0; i < CHILD_COUNT; i++) {
                _children[i]->readDelta(attribute, *reference._children[i], in);
            }
        }  
    }
}

void MetavoxelNode::writeDelta(const AttributePointer& attribute, const MetavoxelNode& reference, Bitstream& out) const {
    bool leaf = isLeaf();
    out << leaf;
    attribute->writeDelta(out, _attributeValue, reference._attributeValue, leaf);
    if (!leaf) {
        if (reference.isLeaf()) {
            for (int i = 0; i < CHILD_COUNT; i++) {
                _children[i]->write(attribute, out);
            }
        } else {
            for (int i = 0; i < CHILD_COUNT; i++) {
                _children[i]->writeDelta(attribute, *reference._children[i], out);
            }
        }
    }
}

void MetavoxelNode::decrementReferenceCount(const AttributePointer& attribute) {
    if (--_referenceCount == 0) {
        destroy(attribute);
        delete this;
    }
}

void MetavoxelNode::destroy(const AttributePointer& attribute) {
    attribute->destroy(_attributeValue);
    for (int i = 0; i < CHILD_COUNT; i++) {
        if (_children[i]) {
            _children[i]->decrementReferenceCount(attribute);
        }
    }
}

void MetavoxelNode::clearChildren(const AttributePointer& attribute) {
    for (int i = 0; i < CHILD_COUNT; i++) {
        if (_children[i]) {
            _children[i]->decrementReferenceCount(attribute);
            _children[i] = NULL;
        }
    }
}

MetavoxelVisitor::MetavoxelVisitor(const QVector<AttributePointer>& inputs, const QVector<AttributePointer>& outputs) :
    _inputs(inputs),
    _outputs(outputs) {
}

MetavoxelVisitor::~MetavoxelVisitor() {
}

PolymorphicData* DefaultMetavoxelGuide::clone() const {
    return new DefaultMetavoxelGuide();
}

const int X_MAXIMUM_FLAG = 1;
const int Y_MAXIMUM_FLAG = 2;
const int Z_MAXIMUM_FLAG = 4;

void DefaultMetavoxelGuide::guide(MetavoxelVisitation& visitation) {
    visitation.info.isLeaf = visitation.allInputNodesLeaves();
    bool keepGoing = visitation.visitor.visit(visitation.info);
    for (int i = 0; i < visitation.outputNodes.size(); i++) {
        AttributeValue& value = visitation.info.outputValues[i];
        if (value.getAttribute()) {
            MetavoxelNode*& node = visitation.outputNodes[i];
            if (!node) {
                node = visitation.createOutputNode(i);
            }
            node->setAttributeValue(value);
        }
    }
    if (!keepGoing) {
        return;
    }
    MetavoxelVisitation nextVisitation = { visitation.data, &visitation, visitation.visitor,
        QVector<MetavoxelNode*>(visitation.inputNodes.size()), QVector<MetavoxelNode*>(visitation.outputNodes.size()),
        { glm::vec3(), visitation.info.size * 0.5f, QVector<AttributeValue>(visitation.inputNodes.size()),
            QVector<AttributeValue>(visitation.outputNodes.size()) } };
    for (int i = 0; i < MetavoxelNode::CHILD_COUNT; i++) {
        for (int j = 0; j < visitation.inputNodes.size(); j++) {
            MetavoxelNode* node = visitation.inputNodes.at(j);
            MetavoxelNode* child = node ? node->getChild(i) : NULL;
            nextVisitation.info.inputValues[j] = ((nextVisitation.inputNodes[j] = child)) ?
                child->getAttributeValue(visitation.info.inputValues[j].getAttribute()) :
                    visitation.info.inputValues[j];
        }
        for (int j = 0; j < visitation.outputNodes.size(); j++) {
            MetavoxelNode* node = visitation.outputNodes.at(j);
            MetavoxelNode* child = node ? node->getChild(i) : NULL;
            nextVisitation.outputNodes[j] = child;
        }
        nextVisitation.info.minimum = visitation.info.minimum + glm::vec3(
            (i & X_MAXIMUM_FLAG) ? nextVisitation.info.size : 0.0f,
            (i & Y_MAXIMUM_FLAG) ? nextVisitation.info.size : 0.0f,
            (i & Z_MAXIMUM_FLAG) ? nextVisitation.info.size : 0.0f);
        nextVisitation.childIndex = i;
        static_cast<MetavoxelGuide*>(nextVisitation.info.inputValues.last().getInlineValue<
            PolymorphicDataPointer>().data())->guide(nextVisitation);
        for (int j = 0; j < nextVisitation.outputNodes.size(); j++) {
            AttributeValue& value = nextVisitation.info.outputValues[j];
            if (value.getAttribute()) {
                visitation.info.outputValues[j] = value;
                value = AttributeValue();
            }
        }
    }
    for (int i = 0; i < visitation.outputNodes.size(); i++) {
        AttributeValue& value = visitation.info.outputValues[i];
        if (value.getAttribute()) {
            MetavoxelNode* node = visitation.outputNodes.at(i);
            node->mergeChildren(value.getAttribute());
            value = node->getAttributeValue(value.getAttribute()); 
        }
    }
}

static QScriptValue getAttributes(QScriptEngine* engine, ScriptedMetavoxelGuide* guide,
        const QVector<AttributePointer>& attributes) {
    
    QScriptValue attributesValue = engine->newArray(attributes.size());
    for (int i = 0; i < attributes.size(); i++) {
        attributesValue.setProperty(i, engine->newQObject(attributes.at(i).data(), QScriptEngine::QtOwnership,
            QScriptEngine::PreferExistingWrapperObject));
    }
    return attributesValue;
}

QScriptValue ScriptedMetavoxelGuide::getInputs(QScriptContext* context, QScriptEngine* engine) {
    ScriptedMetavoxelGuide* guide = static_cast<ScriptedMetavoxelGuide*>(context->callee().data().toVariant().value<void*>());
    return getAttributes(engine, guide, guide->_visitation->visitor.getInputs());
}

QScriptValue ScriptedMetavoxelGuide::getOutputs(QScriptContext* context, QScriptEngine* engine) {
    ScriptedMetavoxelGuide* guide = static_cast<ScriptedMetavoxelGuide*>(context->callee().data().toVariant().value<void*>());
    return getAttributes(engine, guide, guide->_visitation->visitor.getOutputs());
}

QScriptValue ScriptedMetavoxelGuide::visit(QScriptContext* context, QScriptEngine* engine) {
    ScriptedMetavoxelGuide* guide = static_cast<ScriptedMetavoxelGuide*>(context->callee().data().toVariant().value<void*>());

    // start with the basics, including inherited attribute values
    QScriptValue infoValue = context->argument(0);
    QScriptValue minimum = infoValue.property(guide->_minimumHandle);
    MetavoxelInfo info = {
        glm::vec3(minimum.property(0).toNumber(), minimum.property(1).toNumber(), minimum.property(2).toNumber()),
        infoValue.property(guide->_sizeHandle).toNumber(), guide->_visitation->info.inputValues,
        guide->_visitation->info.outputValues, infoValue.property(guide->_isLeafHandle).toBool() };
    
    // extract and convert the values provided by the script
    QScriptValue inputValues = infoValue.property(guide->_inputValuesHandle);
    const QVector<AttributePointer>& inputs = guide->_visitation->visitor.getInputs();
    for (int i = 0; i < inputs.size(); i++) {
        QScriptValue attributeValue = inputValues.property(i);
        if (attributeValue.isValid()) {
            info.inputValues[i] = AttributeValue(inputs.at(i),
                inputs.at(i)->createFromScript(attributeValue, engine));
        }
    }
    
    QScriptValue result = guide->_visitation->visitor.visit(info);
    
    // destroy any created values
    for (int i = 0; i < inputs.size(); i++) {
        if (inputValues.property(i).isValid()) {
            info.inputValues[i].getAttribute()->destroy(info.inputValues[i].getValue());
        }
    }
    
    return result;
}

ScriptedMetavoxelGuide::ScriptedMetavoxelGuide(const QScriptValue& guideFunction) :
    _guideFunction(guideFunction),
    _minimumHandle(guideFunction.engine()->toStringHandle("minimum")),
    _sizeHandle(guideFunction.engine()->toStringHandle("size")),
    _inputValuesHandle(guideFunction.engine()->toStringHandle("inputValues")),
    _outputValuesHandle(guideFunction.engine()->toStringHandle("outputValues")),
    _isLeafHandle(guideFunction.engine()->toStringHandle("isLeaf")),
    _getInputsFunction(guideFunction.engine()->newFunction(getInputs, 0)),
    _getOutputsFunction(guideFunction.engine()->newFunction(getOutputs, 0)),
    _visitFunction(guideFunction.engine()->newFunction(visit, 1)),
    _info(guideFunction.engine()->newObject()),
    _minimum(guideFunction.engine()->newArray(3)) {
    
    _arguments.append(guideFunction.engine()->newObject());
    QScriptValue visitor = guideFunction.engine()->newObject();
    visitor.setProperty("getInputs", _getInputsFunction);
    visitor.setProperty("getOutputs", _getOutputsFunction);
    visitor.setProperty("visit", _visitFunction);
    _arguments[0].setProperty("visitor", visitor);
    _arguments[0].setProperty("info", _info);
    _info.setProperty(_minimumHandle, _minimum);
}

PolymorphicData* ScriptedMetavoxelGuide::clone() const {
    return new ScriptedMetavoxelGuide(_guideFunction);
}

void ScriptedMetavoxelGuide::guide(MetavoxelVisitation& visitation) {
    QScriptValue data = _guideFunction.engine()->newVariant(QVariant::fromValue<void*>(this));
    _getInputsFunction.setData(data);
    _visitFunction.setData(data);
    _minimum.setProperty(0, visitation.info.minimum.x);
    _minimum.setProperty(1, visitation.info.minimum.y);
    _minimum.setProperty(2, visitation.info.minimum.z);
    _info.setProperty(_sizeHandle, visitation.info.size);
    _info.setProperty(_isLeafHandle, visitation.info.isLeaf);
    _visitation = &visitation;
    _guideFunction.call(QScriptValue(), _arguments);
    if (_guideFunction.engine()->hasUncaughtException()) {
        qDebug() << "Script error: " << _guideFunction.engine()->uncaughtException().toString();
    }
}

bool MetavoxelVisitation::allInputNodesLeaves() const {
    foreach (MetavoxelNode* node, inputNodes) {
        if (node != NULL && !node->isLeaf()) {
            return false;
        }
    }
    return true;
}

MetavoxelNode* MetavoxelVisitation::createOutputNode(int index) {
    const AttributePointer& attribute = visitor.getOutputs().at(index);
    if (previous) {
        MetavoxelNode*& parent = previous->outputNodes[index];
        if (!parent) {
            parent = previous->createOutputNode(index);
        }
        AttributeValue value = parent->getAttributeValue(attribute);
        for (int i = 0; i < MetavoxelNode::CHILD_COUNT; i++) {
            parent->_children[i] = new MetavoxelNode(value);
        }
        return parent->_children[childIndex];
        
    } else {    
        return data->_roots[attribute] = new MetavoxelNode(attribute);
    }
}
