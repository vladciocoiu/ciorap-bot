#pragma once

#ifndef MAGICBITBOARDS_H_
#define MAGICBITBOARDS_H_

void initMagics();
void generateMagicNumbers();
U64 magicBishopAttacks(U64 occ, char sq);
U64 magicRookAttacks(U64 occ, char sq);
int popcount(U64 bb);
U64 randomULL();
int bitscanForward(U64 bb);

#endif

