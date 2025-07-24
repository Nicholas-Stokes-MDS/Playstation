﻿/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 12.508.001
* Copyright (C) 2011 Sony Interactive Entertainment Inc.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__

struct VS_OUTPUT
{
	float4 Position		: S_POSITION;
	float3 Color		: TEXCOORD0;
	float2 UV			: TEXCOORD1;
};

#endif