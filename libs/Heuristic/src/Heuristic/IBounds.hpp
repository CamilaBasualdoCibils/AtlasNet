#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
struct IBounds
{
	using BoundsID = uint32_t;
	BoundsID ID;
	virtual void Serialize(ByteWriter&) const = 0;
	virtual void Deserialize(ByteReader&) = 0;

    virtual bool IsInside() const =0;
	auto GetID() const {return ID;}
};