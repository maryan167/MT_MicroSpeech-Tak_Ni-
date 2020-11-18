/*
 * words.h
 *
 *  Created on: Oct 14, 2020
 *      Author: julien
 */

#ifndef FUNCTIONAL_HEADERS_WORDS_H_
#define FUNCTIONAL_HEADERS_WORDS_H_

struct word {
	char word[10];
	int pos;
} word;

const int words_count;
struct word *words;

#endif /* FUNCTIONAL_HEADERS_WORDS_H_ */
