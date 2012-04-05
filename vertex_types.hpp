#pragma once

// Some common vertex types collected in one place..

struct PosNormal
{
	PosNormal() {}
	PosNormal(const XMFLOAT3& pos, const XMFLOAT3& normal) : pos(pos), normal(normal) {}
  PosNormal(float x, float y, float z, float nx, float ny, float nz) : pos(x, y, z), normal(nx, ny, nz) {}
	XMFLOAT3 pos;
	XMFLOAT3 normal;
};

struct Pos2Tex
{
	Pos2Tex() {}
	Pos2Tex(const XMFLOAT2& pos, const XMFLOAT2& tex) : pos(pos), tex(tex) {}
	Pos2Tex(float x, float y, float u, float v) : pos(x, y), tex(u, v) {}
	XMFLOAT2 pos;
	XMFLOAT2 tex;
};

struct PosTex
{
  PosTex() {}
  PosTex(const XMFLOAT3& pos, const XMFLOAT2& tex) : pos(pos), tex(tex) {}
  PosTex(float x, float y, float z, float u, float v) : pos(x, y, z), tex(u, v) {}
  XMFLOAT3 pos;
  XMFLOAT2 tex;
};

struct PosCol
{
  PosCol() {}
  PosCol(float x, float y, float z, float r, float g, float b, float a) : pos(x, y, z), col(r, g, b, a) {}
  PosCol(float x, float y, float z, const XMFLOAT4& col) : pos(x, y, z), col(col) {}
  PosCol(const XMFLOAT3& pos, const XMFLOAT4& col) : pos(pos), col(col) {}
  PosCol(const XMFLOAT2& pos, float z, const XMFLOAT4& col) : pos(pos.x, pos.y, z), col(col) {}
  PosCol(const XMFLOAT3& pos) : pos(pos) {}
  XMFLOAT3 pos;
  XMFLOAT4 col;
};

struct PosNormalTex
{
	PosNormalTex() {}
	PosNormalTex(const XMFLOAT3& pos, const XMFLOAT3& normal, const XMFLOAT2& tex) : pos(pos), normal(normal), tex(tex) {}
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT2 tex;
};
