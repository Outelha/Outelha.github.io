#include "Preset.h"

namespace sfx
{
int effectIdForKey (const juce::String& key)
{
    for (int i = 0; i < numEffects; ++i)
        if (key == effectInfo[static_cast<size_t> (i)].key)
            return i;
    return -1;
}

std::optional<Preset> Preset::fromVar (const juce::var& v, const juce::String& defaultCategory, juce::String* error)
{
    auto fail = [error] (const juce::String& message) -> std::optional<Preset> {
        if (error != nullptr)
            *error = message;
        return std::nullopt;
    };

    if (! v.isObject())
        return fail ("preset is not an object");

    Preset p;
    p.name = v["name"].toString().trim();
    p.description = v["description"].toString();
    p.category = v.hasProperty ("category") ? v["category"].toString() : defaultCategory;
    if (p.name.isEmpty())
        return fail ("preset has no name");

    // Active chain.
    std::array<bool, numEffects> used {};
    if (auto* chain = v["chain"].getArray())
        for (auto& key : *chain)
        {
            const auto id = effectIdForKey (key.toString());
            if (id < 0 || used[static_cast<size_t> (id)])
                return fail (p.name + ": invalid or repeated chain entry '" + key.toString() + "'");
            used[static_cast<size_t> (id)] = true;
            p.chain.push_back (id);
        }

    // Full order: explicit, or the chain followed by the remaining pedals in default order.
    bool haveOrder = false;
    if (auto* order = v["order"].getArray(); order != nullptr && order->size() == numEffects)
    {
        ChainOrder::Order o {};
        for (int i = 0; i < numEffects; ++i)
            o[static_cast<size_t> (i)] = effectIdForKey ((*order)[i].toString());
        if (ChainOrder::isValid (o))
        {
            p.order = o;
            haveOrder = true;
        }
    }
    if (! haveOrder)
    {
        size_t slot = 0;
        for (auto id : p.chain)
            p.order[slot++] = id;
        for (int id = 0; id < numEffects; ++id)
            if (! used[static_cast<size_t> (id)])
                p.order[slot++] = id;
    }

    if (auto* obj = v["params"].getDynamicObject())
        p.params = obj->getProperties();

    return p;
}

juce::var Preset::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("name", name);
    root->setProperty ("category", category);
    root->setProperty ("description", description);

    juce::Array<juce::var> chainKeys, orderKeys;
    for (auto id : chain)
        chainKeys.add (effectInfo[static_cast<size_t> (id)].key);
    for (auto id : order)
        orderKeys.add (effectInfo[static_cast<size_t> (id)].key);
    root->setProperty ("chain", chainKeys);
    root->setProperty ("order", orderKeys);

    auto* paramObj = new juce::DynamicObject();
    for (const auto& nv : params)
        paramObj->setProperty (nv.name, nv.value);
    root->setProperty ("params", juce::var (paramObj));

    return juce::var (root);
}

Preset Preset::capture (juce::AudioProcessorValueTreeState& state, const ChainOrder& chainOrder)
{
    Preset p;
    p.isFactory = false;
    p.order = chainOrder.get();

    for (auto id : p.order)
        if (state.getRawParameterValue (effectInfo[static_cast<size_t> (id)].enabledId)->load() > 0.5f)
            p.chain.push_back (id);

    for (auto* id : allParameterIds)
    {
        auto* param = state.getParameter (id);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            p.params.set (id, choice->getCurrentChoiceName());
        else if (auto* boolean = dynamic_cast<juce::AudioParameterBool*> (param))
            p.params.set (id, boolean->get());
        else if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            p.params.set (id, static_cast<double> (ranged->convertFrom0to1 (ranged->getValue())));
    }
    return p;
}

void Preset::applyTo (juce::AudioProcessorValueTreeState& state, ChainOrder& chainOrder) const
{
    auto setNormalised = [] (juce::RangedAudioParameter& param, float normalised) {
        param.beginChangeGesture();
        param.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
        param.endChangeGesture();
    };

    for (auto* id : allParameterIds)
    {
        auto* param = state.getParameter (id);
        if (param == nullptr)
            continue;

        float normalised = param->getDefaultValue();
        if (const auto* value = params.getVarPointer (id))
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param); choice != nullptr && value->isString())
            {
                const auto index = choice->choices.indexOf (value->toString());
                if (index >= 0)
                    normalised = choice->convertTo0to1 (static_cast<float> (index));
            }
            else if (value->isBool() || value->isInt() || value->isInt64() || value->isDouble())
            {
                normalised = param->convertTo0to1 (static_cast<float> (static_cast<double> (*value)));
            }
        }
        setNormalised (*param, normalised);
    }

    // Pedal on/off comes from the chain.
    std::array<bool, numEffects> active {};
    for (auto id : chain)
        active[static_cast<size_t> (id)] = true;
    for (int i = 0; i < numEffects; ++i)
        if (auto* param = state.getParameter (effectInfo[static_cast<size_t> (i)].enabledId))
            setNormalised (*param, active[static_cast<size_t> (i)] ? 1.0f : 0.0f);

    chainOrder.set (order);
}
} // namespace sfx
