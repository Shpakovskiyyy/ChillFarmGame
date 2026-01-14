#pragma once

UENUM()
enum class ETileNoiseSource : uint8
{
	Height,
	Moisture,
	Secondary,
	HeightPlusSecondary,
};

struct STileNoiseContext
{
	float Height;
	float Moisture;
	float Secondary;
};