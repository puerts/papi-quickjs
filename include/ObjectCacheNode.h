/*
 * Tencent is pleased to support the open source community by making Puerts available.
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.  All rights reserved.
 * Puerts is licensed under the BSD 3-Clause License, except for the third-party components listed in the file 'LICENSE' which may
 * be subject to their corresponding license terms. This file is subject to the terms and conditions defined in file 'LICENSE',
 * which is part of this source code package.
 */

#pragma once

#include "NamespaceDef.h"
#include "quickjs.h"
#include <memory>
namespace puerts
{
namespace qjsimpl
{
class FObjectCacheNode
{
public:
    inline FObjectCacheNode(JSRuntime* RT_, void* TypeId_) : RT(RT_), TypeId(TypeId_), UserData(nullptr), Next(nullptr), MustCallFinalize(false)
    {
    }

    inline FObjectCacheNode(JSRuntime* RT_, const void* TypeId_, FObjectCacheNode* Next_)
        : RT(RT_), TypeId(TypeId_), UserData(nullptr), Next(Next_), MustCallFinalize(false)
    {
    }

    inline FObjectCacheNode(FObjectCacheNode&& other) noexcept
        : RT(other.RT)
        , TypeId(other.TypeId)
        , UserData(other.UserData)
        , Next(other.Next)
        , Value(JS_DupValueRT(other.RT, other.Value))
        , MustCallFinalize(other.MustCallFinalize)
    {
        other.RT = nullptr;
        other.TypeId = nullptr;
        other.UserData = nullptr;
        other.Next = nullptr;
        other.MustCallFinalize = false;
    }

    inline FObjectCacheNode& operator=(FObjectCacheNode&& rhs) noexcept
    {
        RT = rhs.RT;
        TypeId = rhs.TypeId;
        Next = rhs.Next;
        Value = JS_DupValueRT(RT, rhs.Value);
        UserData = rhs.UserData;
        MustCallFinalize = rhs.MustCallFinalize;
        rhs.UserData = nullptr;
        rhs.TypeId = nullptr;
        rhs.Next = nullptr;
        rhs.MustCallFinalize = false;
        return *this;
    }

    ~FObjectCacheNode()
    {
        if (Next)
            delete Next;
        JS_FreeValueRT(RT, Value);
    }

    FObjectCacheNode* Find(const void* TypeId_)
    {
        if (TypeId_ == TypeId)
        {
            return this;
        }
        if (Next)
        {
            return Next->Find(TypeId_);
        }
        return nullptr;
    }

    FObjectCacheNode* Remove(const void* TypeId_, bool IsHead)
    {
        if (TypeId_ == TypeId)
        {
            if (IsHead)
            {
                if (Next)
                {
                    auto PreNext = Next;
                    *this = std::move(*Next);
                    delete PreNext;
                }
                else
                {
                    TypeId = nullptr;
                    Next = nullptr;
                    JS_FreeValueRT(RT, Value);
                }
            }
            return this;
        }
        if (Next)
        {
            auto Removed = Next->Remove(TypeId_, false);
            if (Removed && Removed == Next)    // detach & delete by prev node
            {
                Next = Removed->Next;
                Removed->Next = nullptr;
                delete Removed;
            }
            return Removed;
        }
        return nullptr;
    }

    inline FObjectCacheNode* Add(const void* TypeId_)
    {
        Next = new FObjectCacheNode(RT, TypeId_, Next);
        return Next;
    }

    JSRuntime* RT;

    const void* TypeId;

    void* UserData;

    FObjectCacheNode* Next;

    JSValue Value;

    bool MustCallFinalize;

    FObjectCacheNode(const FObjectCacheNode&) = delete;
    void operator=(const FObjectCacheNode&) = delete;
};

}    // namespace qjsimpl
}    // namespace pesapi