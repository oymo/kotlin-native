// TODO: Consider reformatting this file.
// clang-format on
/*
 * Copyright 2010-2017 JetBrains s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>

#include "City.h"
#include "Exceptions.h"
#include "KAssert.h"
#include "KString.h"
#include "Memory.h"
#include "Natives.h"
#include "Porting.h"
#include "Types.h"

#include "utf8.h"

namespace {

typedef std::back_insert_iterator<KStdString> KStdStringInserter;
typedef KChar* utf8to16(const char*, const char*, KChar*);
typedef KStdStringInserter utf16to8(const KChar*, const KChar*, KStdStringInserter);

KStdStringInserter utf16toUtf8OrThrow(const KChar* start, const KChar* end, KStdStringInserter result) {
    TRY_CATCH(
        result = utf8::utf16to8(start, end, result), result = utf8::unchecked::utf16to8(start, end, result),
        ThrowCharacterCodingException());
    return result;
}

template <utf8to16 conversion> OBJ_GETTER(utf8ToUtf16Impl, const char* rawString, const char* end, uint32_t charCount) {
    if (rawString == nullptr)
        RETURN_OBJ(nullptr);
    ArrayHeader* result = AllocArrayInstance(theStringTypeInfo, charCount, OBJ_RESULT)->array();
    KChar* rawResult = CharArrayAddressOfElementAt(result, 0);
    auto convertResult = conversion(rawString, end, rawResult);
    RETURN_OBJ(result->obj());
}

template <utf16to8 conversion> OBJ_GETTER(unsafeUtf16ToUtf8Impl, KString thiz, KInt start, KInt size) {
    RuntimeAssert(thiz->type_info() == theStringTypeInfo, "Must use String");
    const KChar* utf16 = CharArrayAddressOfElementAt(thiz, start);
    KStdString utf8;
    utf8.reserve(size);
    conversion(utf16, utf16 + size, back_inserter(utf8));
    ArrayHeader* result = AllocArrayInstance(theByteArrayTypeInfo, utf8.size(), OBJ_RESULT)->array();
    ::memcpy(ByteArrayAddressOfElementAt(result, 0), utf8.c_str(), utf8.size());
    RETURN_OBJ(result->obj());
}

OBJ_GETTER(utf8ToUtf16OrThrow, const char* rawString, size_t rawStringLength) {
    const char* end = rawString + rawStringLength;
    uint32_t charCount;
    TRY_CATCH(
        charCount = utf8::utf16_length(rawString, end), charCount = utf8::unchecked::utf16_length(rawString, end),
        ThrowCharacterCodingException());
    RETURN_RESULT_OF(utf8ToUtf16Impl<utf8::unchecked::utf8to16>, rawString, end, charCount);
}

OBJ_GETTER(utf8ToUtf16, const char* rawString, size_t rawStringLength) {
    const char* end = rawString + rawStringLength;
    uint32_t charCount = utf8::with_replacement::utf16_length(rawString, end);
    RETURN_RESULT_OF(utf8ToUtf16Impl<utf8::with_replacement::utf8to16>, rawString, end, charCount);
}


// Case conversion is derived work from Apache Harmony.
// Unicode 3.0.1 (same as Unicode 3.0.0)
enum CharacterClass {
    /**
     * Unicode category constant Cn.
     */
    UNASSIGNED = 0,
    /**
     * Unicode category constant Lu.
     */
    UPPERCASE_LETTER = 1,
    /**
     * Unicode category constant Ll.
     */
    LOWERCASE_LETTER = 2,
    /**
     * Unicode category constant Lt.
     */
    TITLECASE_LETTER = 3,
    /**
     * Unicode category constant Lm.
     */
    MODIFIER_LETTER = 4,
    /**
     * Unicode category constant Lo.
     */
    OTHER_LETTER = 5,
    /**
     * Unicode category constant Mn.
     */
    NON_SPACING_MARK = 6,
    /**
     * Unicode category constant Me.
     */
    ENCLOSING_MARK = 7,
    /**
     * Unicode category constant Mc.
     */
    COMBINING_SPACING_MARK = 8,
    /**
     * Unicode category constant Nd.
     */
    DECIMAL_DIGIT_NUMBER = 9,
    /**
     * Unicode category constant Nl.
     */
    LETTER_NUMBER = 10,
    /**
     * Unicode category constant No.
     */
    OTHER_NUMBER = 11,
    /**
     * Unicode category constant Zs.
     */
    SPACE_SEPARATOR = 12,
    /**
     * Unicode category constant Zl.
     */
    LINE_SEPARATOR = 13,
    /**
     * Unicode category constant Zp.
     */
    PARAGRAPH_SEPARATOR = 14,
    /**
     * Unicode category constant Cc.
     */
    CONTROL = 15,
    /**
     * Unicode category constant Cf.
     */
    FORMAT = 16,
    /**
     * Unicode category constant Co.
     */
    PRIVATE_USE = 18,
    /**
     * Unicode category constant Cs.
     */
    SURROGATE = 19,
    /**
     * Unicode category constant Pd.
     */
    DASH_PUNCTUATION = 20,
    /**
     * Unicode category constant Ps.
     */
    START_PUNCTUATION = 21,
    /**
     * Unicode category constant Pe.
     */
    END_PUNCTUATION = 22,
    /**
     * Unicode category constant Pc.
     */
    CONNECTOR_PUNCTUATION = 23,
    /**
     * Unicode category constant Po.
     */
    OTHER_PUNCTUATION = 24,
    /**
     * Unicode category constant Sm.
     */
    MATH_SYMBOL = 25,
    /**
     * Unicode category constant Sc.
     */
    CURRENCY_SYMBOL = 26,
    /**
     * Unicode category constant Sk.
     */
    MODIFIER_SYMBOL = 27,
    /**
     * Unicode category constant So.
     */
    OTHER_SYMBOL = 28,
    /**
     * Unicode category constant Pi.
     */
    INITIAL_QUOTE_PUNCTUATION = 29,
    /**
     * Unicode category constant Pf.
     */
    FINAL_QUOTE_PUNCTUATION = 30
};

constexpr KByte typeValuesCache[] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 12, 24, 24, 24, 26, 24, 24, 24, 21, 22, 24, 25, 24, 20, 24, 24, 9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
    24, 24, 25, 25, 25, 24, 24, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  21, 24, 22, 27, 23, 27, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  21, 25, 22, 25, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 12, 24, 26, 26, 26, 26, 28, 28, 27, 28, 2,  29, 25, 16,
    28, 27, 28, 25, 11, 11, 27, 2,  28, 24, 27, 11, 2,  30, 11, 11, 11, 24, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  25, 1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  25, 2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  1,  2,  1,
    2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,
    1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  2,  1,  2,  1,  2,  1,  2,
    1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,
    1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,
    1,  2,  1,  2,  1,  2,  2,  2,  1,  1,  2,  1,  2,  1,  1,  2,  1,  1,  1,  2,  2,  1,  1,  1,  1,  2,  1,  1,  2,
    1,  1,  1,  2,  2,  2,  1,  1,  2,  1,  1,  2,  1,  2,  1,  2,  1,  1,  2,  1,  2,  2,  1,  2,  1,  1,  2,  1,  1,
    1,  2,  1,  2,  1,  1,  2,  2,  5,  1,  2,  2,  2,  5,  5,  5,  5,  1,  3,  2,  1,  3,  2,  1,  3,  2,  1,  2,  1,
    2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,
    2,  1,  2,  2,  1,  3,  2,  1,  2,  1,  1,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,
    1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,
    2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  4,  4,  4,  4,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  27, 27, 27, 27, 4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  27, 27, 27,
    27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 4,  4,  4,  4,  4,  27, 27, 27, 27, 27, 27, 27, 27, 27, 4,  27, 27, 27,
    27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  0,  0,  0,  0,  0,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  0,  0,  0,  0,  27, 27, 0,  0,  0,  0,  4,  0,  0,  0,  24, 0,  0,  0,  0,
    0,  27, 27, 1,  24, 1,  1,  1,  0,  1,  0,  1,  1,  2,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  0,  2,  2,  1,  1,  1,  2,  2,  2,  1,  2,
    1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2};

constexpr KShort uppercaseValuesCache[] = {
    924, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203,
    204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 247, 216, 217,
    218, 219, 220, 221, 222, 376, 256, 256, 258, 258, 260, 260, 262, 262, 264, 264, 266, 266, 268, 268, 270, 270, 272,
    272, 274, 274, 276, 276, 278, 278, 280, 280, 282, 282, 284, 284, 286, 286, 288, 288, 290, 290, 292, 292, 294, 294,
    296, 296, 298, 298, 300, 300, 302, 302, 304, 73,  306, 306, 308, 308, 310, 310, 312, 313, 313, 315, 315, 317, 317,
    319, 319, 321, 321, 323, 323, 325, 325, 327, 327, 329, 330, 330, 332, 332, 334, 334, 336, 336, 338, 338, 340, 340,
    342, 342, 344, 344, 346, 346, 348, 348, 350, 350, 352, 352, 354, 354, 356, 356, 358, 358, 360, 360, 362, 362, 364,
    364, 366, 366, 368, 368, 370, 370, 372, 372, 374, 374, 376, 377, 377, 379, 379, 381, 381, 83,  384, 385, 386, 386,
    388, 388, 390, 391, 391, 393, 394, 395, 395, 397, 398, 399, 400, 401, 401, 403, 404, 502, 406, 407, 408, 408, 410,
    411, 412, 413, 544, 415, 416, 416, 418, 418, 420, 420, 422, 423, 423, 425, 426, 427, 428, 428, 430, 431, 431, 433,
    434, 435, 435, 437, 437, 439, 440, 440, 442, 443, 444, 444, 446, 503, 448, 449, 450, 451, 452, 452, 452, 455, 455,
    455, 458, 458, 458, 461, 461, 463, 463, 465, 465, 467, 467, 469, 469, 471, 471, 473, 473, 475, 475, 398, 478, 478,
    480, 480, 482, 482, 484, 484, 486, 486, 488, 488, 490, 490, 492, 492, 494, 494, 496, 497, 497, 497, 500, 500, 502,
    503, 504, 504, 506, 506, 508, 508, 510, 510, 512, 512, 514, 514, 516, 516, 518, 518, 520, 520, 522, 522, 524, 524,
    526, 526, 528, 528, 530, 530, 532, 532, 534, 534, 536, 536, 538, 538, 540, 540, 542, 542, 544, 545, 546, 546, 548,
    548, 550, 550, 552, 552, 554, 554, 556, 556, 558, 558, 560, 560, 562, 562, 564, 565, 566, 567, 568, 569, 570, 571,
    572, 573, 574, 575, 576, 577, 578, 579, 580, 581, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594,
    385, 390, 597, 393, 394, 600, 399, 602, 400, 604, 605, 606, 607, 403, 609, 610, 404, 612, 613, 614, 615, 407, 406,
    618, 619, 620, 621, 622, 412, 624, 625, 413, 627, 628, 415, 630, 631, 632, 633, 634, 635, 636, 637, 638, 639, 422,
    641, 642, 425, 644, 645, 646, 647, 430, 649, 433, 434, 652, 653, 654, 655, 656, 657, 439, 659, 660, 661, 662, 663,
    664, 665, 666, 667, 668, 669, 670, 671, 672, 673, 674, 675, 676, 677, 678, 679, 680, 681, 682, 683, 684, 685, 686,
    687, 688, 689, 690, 691, 692, 693, 694, 695, 696, 697, 698, 699, 700, 701, 702, 703, 704, 705, 706, 707, 708, 709,
    710, 711, 712, 713, 714, 715, 716, 717, 718, 719, 720, 721, 722, 723, 724, 725, 726, 727, 728, 729, 730, 731, 732,
    733, 734, 735, 736, 737, 738, 739, 740, 741, 742, 743, 744, 745, 746, 747, 748, 749, 750, 751, 752, 753, 754, 755,
    756, 757, 758, 759, 760, 761, 762, 763, 764, 765, 766, 767, 768, 769, 770, 771, 772, 773, 774, 775, 776, 777, 778,
    779, 780, 781, 782, 783, 784, 785, 786, 787, 788, 789, 790, 791, 792, 793, 794, 795, 796, 797, 798, 799, 800, 801,
    802, 803, 804, 805, 806, 807, 808, 809, 810, 811, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822, 823, 824,
    825, 826, 827, 828, 829, 830, 831, 832, 833, 834, 835, 836, 921, 838, 839, 840, 841, 842, 843, 844, 845, 846, 847,
    848, 849, 850, 851, 852, 853, 854, 855, 856, 857, 858, 859, 860, 861, 862, 863, 864, 865, 866, 867, 868, 869, 870,
    871, 872, 873, 874, 875, 876, 877, 878, 879, 880, 881, 882, 883, 884, 885, 886, 887, 888, 889, 890, 891, 892, 893,
    894, 895, 896, 897, 898, 899, 900, 901, 902, 903, 904, 905, 906, 907, 908, 909, 910, 911, 912, 913, 914, 915, 916,
    917, 918, 919, 920, 921, 922, 923, 924, 925, 926, 927, 928, 929, 930, 931, 932, 933, 934, 935, 936, 937, 938, 939,
    902, 904, 905, 906, 944, 913, 914, 915, 916, 917, 918, 919, 920, 921, 922, 923, 924, 925, 926, 927, 928, 929, 931,
    931, 932, 933, 934, 935, 936, 937, 938, 939, 908, 910, 911, 975, 914, 920, 978, 979, 980, 934, 928, 983, 984, 984,
    986, 986, 988, 988, 990, 990, 992, 992, 994, 994, 996, 996, 998, 998};

constexpr KShort lowercaseValuesCache[] = {
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
    215, 248, 249, 250, 251, 252, 253, 254, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237,
    238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 257, 257, 259, 259, 261,
    261, 263, 263, 265, 265, 267, 267, 269, 269, 271, 271, 273, 273, 275, 275, 277, 277, 279, 279, 281, 281, 283, 283,
    285, 285, 287, 287, 289, 289, 291, 291, 293, 293, 295, 295, 297, 297, 299, 299, 301, 301, 303, 303, 105, 305, 307,
    307, 309, 309, 311, 311, 312, 314, 314, 316, 316, 318, 318, 320, 320, 322, 322, 324, 324, 326, 326, 328, 328, 329,
    331, 331, 333, 333, 335, 335, 337, 337, 339, 339, 341, 341, 343, 343, 345, 345, 347, 347, 349, 349, 351, 351, 353,
    353, 355, 355, 357, 357, 359, 359, 361, 361, 363, 363, 365, 365, 367, 367, 369, 369, 371, 371, 373, 373, 375, 375,
    255, 378, 378, 380, 380, 382, 382, 383, 384, 595, 387, 387, 389, 389, 596, 392, 392, 598, 599, 396, 396, 397, 477,
    601, 603, 402, 402, 608, 611, 405, 617, 616, 409, 409, 410, 411, 623, 626, 414, 629, 417, 417, 419, 419, 421, 421,
    640, 424, 424, 643, 426, 427, 429, 429, 648, 432, 432, 650, 651, 436, 436, 438, 438, 658, 441, 441, 442, 443, 445,
    445, 446, 447, 448, 449, 450, 451, 454, 454, 454, 457, 457, 457, 460, 460, 460, 462, 462, 464, 464, 466, 466, 468,
    468, 470, 470, 472, 472, 474, 474, 476, 476, 477, 479, 479, 481, 481, 483, 483, 485, 485, 487, 487, 489, 489, 491,
    491, 493, 493, 495, 495, 496, 499, 499, 499, 501, 501, 405, 447, 505, 505, 507, 507, 509, 509, 511, 511, 513, 513,
    515, 515, 517, 517, 519, 519, 521, 521, 523, 523, 525, 525, 527, 527, 529, 529, 531, 531, 533, 533, 535, 535, 537,
    537, 539, 539, 541, 541, 543, 543, 414, 545, 547, 547, 549, 549, 551, 551, 553, 553, 555, 555, 557, 557, 559, 559,
    561, 561, 563, 563, 564, 565, 566, 567, 568, 569, 570, 571, 572, 573, 574, 575, 576, 577, 578, 579, 580, 581, 582,
    583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602, 603, 604, 605,
    606, 607, 608, 609, 610, 611, 612, 613, 614, 615, 616, 617, 618, 619, 620, 621, 622, 623, 624, 625, 626, 627, 628,
    629, 630, 631, 632, 633, 634, 635, 636, 637, 638, 639, 640, 641, 642, 643, 644, 645, 646, 647, 648, 649, 650, 651,
    652, 653, 654, 655, 656, 657, 658, 659, 660, 661, 662, 663, 664, 665, 666, 667, 668, 669, 670, 671, 672, 673, 674,
    675, 676, 677, 678, 679, 680, 681, 682, 683, 684, 685, 686, 687, 688, 689, 690, 691, 692, 693, 694, 695, 696, 697,
    698, 699, 700, 701, 702, 703, 704, 705, 706, 707, 708, 709, 710, 711, 712, 713, 714, 715, 716, 717, 718, 719, 720,
    721, 722, 723, 724, 725, 726, 727, 728, 729, 730, 731, 732, 733, 734, 735, 736, 737, 738, 739, 740, 741, 742, 743,
    744, 745, 746, 747, 748, 749, 750, 751, 752, 753, 754, 755, 756, 757, 758, 759, 760, 761, 762, 763, 764, 765, 766,
    767, 768, 769, 770, 771, 772, 773, 774, 775, 776, 777, 778, 779, 780, 781, 782, 783, 784, 785, 786, 787, 788, 789,
    790, 791, 792, 793, 794, 795, 796, 797, 798, 799, 800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 810, 811, 812,
    813, 814, 815, 816, 817, 818, 819, 820, 821, 822, 823, 824, 825, 826, 827, 828, 829, 830, 831, 832, 833, 834, 835,
    836, 837, 838, 839, 840, 841, 842, 843, 844, 845, 846, 847, 848, 849, 850, 851, 852, 853, 854, 855, 856, 857, 858,
    859, 860, 861, 862, 863, 864, 865, 866, 867, 868, 869, 870, 871, 872, 873, 874, 875, 876, 877, 878, 879, 880, 881,
    882, 883, 884, 885, 886, 887, 888, 889, 890, 891, 892, 893, 894, 895, 896, 897, 898, 899, 900, 901, 940, 903, 941,
    942, 943, 907, 972, 909, 973, 974, 912, 945, 946, 947, 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959,
    960, 961, 930, 963, 964, 965, 966, 967, 968, 969, 970, 971, 940, 941, 942, 943, 944, 945, 946, 947, 948, 949, 950,
    951, 952, 953, 954, 955, 956, 957, 958, 959, 960, 961, 962, 963, 964, 965, 966, 967, 968, 969, 970, 971, 972, 973,
    974, 975, 976, 977, 978, 979, 980, 981, 982, 983, 985, 985, 987, 987, 989, 989, 991, 991, 993, 993, 995, 995, 997,
    997, 999, 999};

constexpr KChar uppercaseKeys[] = {
    0x61,   0xb5,   0xe0,   0xf8,   0xff,   0x101,  0x131,  0x133,  0x13a,  0x14b,  0x17a,  0x17f,  0x183,
    0x188,  0x18c,  0x192,  0x195,  0x199,  0x1a1,  0x1a8,  0x1ad,  0x1b0,  0x1b4,  0x1b9,  0x1bd,  0x1bf,
    0x1c5,  0x1c6,  0x1c8,  0x1c9,  0x1cb,  0x1cc,  0x1ce,  0x1dd,  0x1df,  0x1f2,  0x1f3,  0x1f5,  0x1f9,
    0x223,  0x253,  0x254,  0x256,  0x259,  0x25b,  0x260,  0x263,  0x268,  0x269,  0x26f,  0x272,  0x275,
    0x280,  0x283,  0x288,  0x28a,  0x292,  0x345,  0x3ac,  0x3ad,  0x3b1,  0x3c2,  0x3c3,  0x3cc,  0x3cd,
    0x3d0,  0x3d1,  0x3d5,  0x3d6,  0x3db,  0x3f0,  0x3f1,  0x3f2,  0x430,  0x450,  0x461,  0x48d,  0x4c2,
    0x4c8,  0x4cc,  0x4d1,  0x4f9,  0x561,  0x1e01, 0x1e9b, 0x1ea1, 0x1f00, 0x1f10, 0x1f20, 0x1f30, 0x1f40,
    0x1f51, 0x1f60, 0x1f70, 0x1f72, 0x1f76, 0x1f78, 0x1f7a, 0x1f7c, 0x1f80, 0x1f90, 0x1fa0, 0x1fb0, 0x1fb3,
    0x1fbe, 0x1fc3, 0x1fd0, 0x1fe0, 0x1fe5, 0x1ff3, 0x2170, 0x24d0, 0xff41};

constexpr KChar uppercaseValues[] = {
    0x7a,   0xffe0, 0xb5,   0x2e7,  0xf6,   0xffe0, 0xfe,   0xffe0, 0xff,   0x79,   0x812f, 0xffff, 0x131,  0xff18,
    0x8137, 0xffff, 0x8148, 0xffff, 0x8177, 0xffff, 0x817e, 0xffff, 0x17f,  0xfed4, 0x8185, 0xffff, 0x188,  0xffff,
    0x18c,  0xffff, 0x192,  0xffff, 0x195,  0x61,   0x199,  0xffff, 0x81a5, 0xffff, 0x1a8,  0xffff, 0x1ad,  0xffff,
    0x1b0,  0xffff, 0x81b6, 0xffff, 0x1b9,  0xffff, 0x1bd,  0xffff, 0x1bf,  0x38,   0x1c5,  0xffff, 0x1c6,  0xfffe,
    0x1c8,  0xffff, 0x1c9,  0xfffe, 0x1cb,  0xffff, 0x1cc,  0xfffe, 0x81dc, 0xffff, 0x1dd,  0xffb1, 0x81ef, 0xffff,
    0x1f2,  0xffff, 0x1f3,  0xfffe, 0x1f5,  0xffff, 0x821f, 0xffff, 0x8233, 0xffff, 0x253,  0xff2e, 0x254,  0xff32,
    0x257,  0xff33, 0x259,  0xff36, 0x25b,  0xff35, 0x260,  0xff33, 0x263,  0xff31, 0x268,  0xff2f, 0x269,  0xff2d,
    0x26f,  0xff2d, 0x272,  0xff2b, 0x275,  0xff2a, 0x280,  0xff26, 0x283,  0xff26, 0x288,  0xff26, 0x28b,  0xff27,
    0x292,  0xff25, 0x345,  0x54,   0x3ac,  0xffda, 0x3af,  0xffdb, 0x3c1,  0xffe0, 0x3c2,  0xffe1, 0x3cb,  0xffe0,
    0x3cc,  0xffc0, 0x3ce,  0xffc1, 0x3d0,  0xffc2, 0x3d1,  0xffc7, 0x3d5,  0xffd1, 0x3d6,  0xffca, 0x83ef, 0xffff,
    0x3f0,  0xffaa, 0x3f1,  0xffb0, 0x3f2,  0xffb1, 0x44f,  0xffe0, 0x45f,  0xffb0, 0x8481, 0xffff, 0x84bf, 0xffff,
    0x84c4, 0xffff, 0x4c8,  0xffff, 0x4cc,  0xffff, 0x84f5, 0xffff, 0x4f9,  0xffff, 0x586,  0xffd0, 0x9e95, 0xffff,
    0x1e9b, 0xffc5, 0x9ef9, 0xffff, 0x1f07, 0x8,    0x1f15, 0x8,    0x1f27, 0x8,    0x1f37, 0x8,    0x1f45, 0x8,
    0x9f57, 0x8,    0x1f67, 0x8,    0x1f71, 0x4a,   0x1f75, 0x56,   0x1f77, 0x64,   0x1f79, 0x80,   0x1f7b, 0x70,
    0x1f7d, 0x7e,   0x1f87, 0x8,    0x1f97, 0x8,    0x1fa7, 0x8,    0x1fb1, 0x8,    0x1fb3, 0x9,    0x1fbe, 0xe3db,
    0x1fc3, 0x9,    0x1fd1, 0x8,    0x1fe1, 0x8,    0x1fe5, 0x7,    0x1ff3, 0x9,    0x217f, 0xfff0, 0x24e9, 0xffe6,
    0xff5a, 0xffe0};

constexpr KChar lowercaseKeys[] = {
    0x41,   0xc0,   0xd8,   0x100,  0x130,  0x132,  0x139,  0x14a,  0x178,  0x179,  0x181,  0x182,  0x186,
    0x187,  0x189,  0x18b,  0x18e,  0x18f,  0x190,  0x191,  0x193,  0x194,  0x196,  0x197,  0x198,  0x19c,
    0x19d,  0x19f,  0x1a0,  0x1a6,  0x1a7,  0x1a9,  0x1ac,  0x1ae,  0x1af,  0x1b1,  0x1b3,  0x1b7,  0x1b8,
    0x1bc,  0x1c4,  0x1c5,  0x1c7,  0x1c8,  0x1ca,  0x1cb,  0x1de,  0x1f1,  0x1f2,  0x1f6,  0x1f7,  0x1f8,
    0x222,  0x386,  0x388,  0x38c,  0x38e,  0x391,  0x3a3,  0x3da,  0x400,  0x410,  0x460,  0x48c,  0x4c1,
    0x4c7,  0x4cb,  0x4d0,  0x4f8,  0x531,  0x1e00, 0x1ea0, 0x1f08, 0x1f18, 0x1f28, 0x1f38, 0x1f48, 0x1f59,
    0x1f68, 0x1f88, 0x1f98, 0x1fa8, 0x1fb8, 0x1fba, 0x1fbc, 0x1fc8, 0x1fcc, 0x1fd8, 0x1fda, 0x1fe8, 0x1fea,
    0x1fec, 0x1ff8, 0x1ffa, 0x1ffc, 0x2126, 0x212a, 0x212b, 0x2160, 0x24b6};

constexpr KChar lowercaseValues[] = {
    0x5a,   0x20,   0xd6,   0x20,   0xde,   0x20,   0x812e, 0x1,    0x130,  0xff39, 0x8136, 0x1,    0x8147, 0x1,
    0x8176, 0x1,    0x178,  0xff87, 0x817d, 0x1,    0x181,  0xd2,   0x8184, 0x1,    0x186,  0xce,   0x187,  0x1,
    0x18a,  0xcd,   0x18b,  0x1,    0x18e,  0x4f,   0x18f,  0xca,   0x190,  0xcb,   0x191,  0x1,    0x193,  0xcd,
    0x194,  0xcf,   0x196,  0xd3,   0x197,  0xd1,   0x198,  0x1,    0x19c,  0xd3,   0x19d,  0xd5,   0x19f,  0xd6,
    0x81a4, 0x1,    0x1a6,  0xda,   0x1a7,  0x1,    0x1a9,  0xda,   0x1ac,  0x1,    0x1ae,  0xda,   0x1af,  0x1,
    0x1b2,  0xd9,   0x81b5, 0x1,    0x1b7,  0xdb,   0x1b8,  0x1,    0x1bc,  0x1,    0x1c4,  0x2,    0x1c5,  0x1,
    0x1c7,  0x2,    0x1c8,  0x1,    0x1ca,  0x2,    0x81db, 0x1,    0x81ee, 0x1,    0x1f1,  0x2,    0x81f4, 0x1,
    0x1f6,  0xff9f, 0x1f7,  0xffc8, 0x821e, 0x1,    0x8232, 0x1,    0x386,  0x26,   0x38a,  0x25,   0x38c,  0x40,
    0x38f,  0x3f,   0x3a1,  0x20,   0x3ab,  0x20,   0x83ee, 0x1,    0x40f,  0x50,   0x42f,  0x20,   0x8480, 0x1,
    0x84be, 0x1,    0x84c3, 0x1,    0x4c7,  0x1,    0x4cb,  0x1,    0x84f4, 0x1,    0x4f8,  0x1,    0x556,  0x30,
    0x9e94, 0x1,    0x9ef8, 0x1,    0x1f0f, 0xfff8, 0x1f1d, 0xfff8, 0x1f2f, 0xfff8, 0x1f3f, 0xfff8, 0x1f4d, 0xfff8,
    0x9f5f, 0xfff8, 0x1f6f, 0xfff8, 0x1f8f, 0xfff8, 0x1f9f, 0xfff8, 0x1faf, 0xfff8, 0x1fb9, 0xfff8, 0x1fbb, 0xffb6,
    0x1fbc, 0xfff7, 0x1fcb, 0xffaa, 0x1fcc, 0xfff7, 0x1fd9, 0xfff8, 0x1fdb, 0xff9c, 0x1fe9, 0xfff8, 0x1feb, 0xff90,
    0x1fec, 0xfff9, 0x1ff9, 0xff80, 0x1ffb, 0xff82, 0x1ffc, 0xfff7, 0x2126, 0xe2a3, 0x212a, 0xdf41, 0x212b, 0xdfba,
    0x216f, 0x10,   0x24cf, 0x1a};

constexpr KChar digitKeys[] = {0x30,  0x41,   0x61,   0x660,  0x6f0,  0x966,  0x9e6,  0xa66,
                               0xae6, 0xb66,  0xbe7,  0xc66,  0xce6,  0xd66,  0xe50,  0xed0,
                               0xf20, 0x1040, 0x1369, 0x17e0, 0x1810, 0xff10, 0xff21, 0xff41};

constexpr KChar digitValues[] = {0x39,   0x30,   0x5a,   0x37,   0x7a,   0x57,   0x669,  0x660,  0x6f9,  0x6f0,
                                 0x96f,  0x966,  0x9ef,  0x9e6,  0xa6f,  0xa66,  0xaef,  0xae6,  0xb6f,  0xb66,
                                 0xbef,  0xbe6,  0xc6f,  0xc66,  0xcef,  0xce6,  0xd6f,  0xd66,  0xe59,  0xe50,
                                 0xed9,  0xed0,  0xf29,  0xf20,  0x1049, 0x1040, 0x1371, 0x1368, 0x17e9, 0x17e0,
                                 0x1819, 0x1810, 0xff19, 0xff10, 0xff3a, 0xff17, 0xff5a, 0xff37};

constexpr KChar typeKeys[] = {
    0x0,    0x20,   0x22,   0x24,   0x26,   0x28,   0x2a,   0x2d,   0x2f,   0x31,   0x3a,   0x3c,   0x3f,   0x41,
    0x5b,   0x5d,   0x5f,   0x61,   0x7b,   0x7d,   0x7f,   0xa0,   0xa2,   0xa6,   0xa8,   0xaa,   0xac,   0xae,
    0xb1,   0xb3,   0xb5,   0xb7,   0xb9,   0xbb,   0xbd,   0xbf,   0xc1,   0xd7,   0xd9,   0xdf,   0xf7,   0xf9,
    0x100,  0x138,  0x149,  0x179,  0x17f,  0x181,  0x183,  0x187,  0x18a,  0x18c,  0x18e,  0x192,  0x194,  0x197,
    0x199,  0x19c,  0x19e,  0x1a0,  0x1a7,  0x1ab,  0x1af,  0x1b2,  0x1b4,  0x1b8,  0x1ba,  0x1bc,  0x1be,  0x1c0,
    0x1c4,  0x1c6,  0x1c8,  0x1ca,  0x1cc,  0x1dd,  0x1f0,  0x1f2,  0x1f4,  0x1f7,  0x1f9,  0x222,  0x250,  0x2b0,
    0x2b9,  0x2bb,  0x2c2,  0x2d0,  0x2d2,  0x2e0,  0x2e5,  0x2ee,  0x300,  0x360,  0x374,  0x37a,  0x37e,  0x384,
    0x386,  0x389,  0x38c,  0x38e,  0x390,  0x392,  0x3a3,  0x3ac,  0x3d0,  0x3d2,  0x3d5,  0x3da,  0x3f0,  0x400,
    0x430,  0x460,  0x482,  0x484,  0x488,  0x48c,  0x4c1,  0x4c7,  0x4cb,  0x4d0,  0x4f8,  0x531,  0x559,  0x55b,
    0x561,  0x589,  0x591,  0x5a3,  0x5bb,  0x5be,  0x5c2,  0x5d0,  0x5f0,  0x5f3,  0x60c,  0x61b,  0x61f,  0x621,
    0x640,  0x642,  0x64b,  0x660,  0x66a,  0x670,  0x672,  0x6d4,  0x6d6,  0x6dd,  0x6df,  0x6e5,  0x6e7,  0x6e9,
    0x6eb,  0x6f0,  0x6fa,  0x6fd,  0x700,  0x70f,  0x711,  0x713,  0x730,  0x780,  0x7a6,  0x901,  0x903,  0x905,
    0x93c,  0x93e,  0x941,  0x949,  0x94d,  0x950,  0x952,  0x958,  0x962,  0x964,  0x966,  0x970,  0x981,  0x983,
    0x985,  0x98f,  0x993,  0x9aa,  0x9b2,  0x9b6,  0x9bc,  0x9be,  0x9c1,  0x9c7,  0x9cb,  0x9cd,  0x9d7,  0x9dc,
    0x9df,  0x9e2,  0x9e6,  0x9f0,  0x9f2,  0x9f4,  0x9fa,  0xa02,  0xa05,  0xa0f,  0xa13,  0xa2a,  0xa32,  0xa35,
    0xa38,  0xa3c,  0xa3e,  0xa41,  0xa47,  0xa4b,  0xa59,  0xa5e,  0xa66,  0xa70,  0xa72,  0xa81,  0xa83,  0xa85,
    0xa8d,  0xa8f,  0xa93,  0xaaa,  0xab2,  0xab5,  0xabc,  0xabe,  0xac1,  0xac7,  0xac9,  0xacb,  0xacd,  0xad0,
    0xae0,  0xae6,  0xb01,  0xb03,  0xb05,  0xb0f,  0xb13,  0xb2a,  0xb32,  0xb36,  0xb3c,  0xb3e,  0xb42,  0xb47,
    0xb4b,  0xb4d,  0xb56,  0xb5c,  0xb5f,  0xb66,  0xb70,  0xb82,  0xb85,  0xb8e,  0xb92,  0xb99,  0xb9c,  0xb9e,
    0xba3,  0xba8,  0xbae,  0xbb7,  0xbbe,  0xbc0,  0xbc2,  0xbc6,  0xbca,  0xbcd,  0xbd7,  0xbe7,  0xbf0,  0xc01,
    0xc05,  0xc0e,  0xc12,  0xc2a,  0xc35,  0xc3e,  0xc41,  0xc46,  0xc4a,  0xc55,  0xc60,  0xc66,  0xc82,  0xc85,
    0xc8e,  0xc92,  0xcaa,  0xcb5,  0xcbe,  0xcc1,  0xcc6,  0xcc8,  0xcca,  0xccc,  0xcd5,  0xcde,  0xce0,  0xce6,
    0xd02,  0xd05,  0xd0e,  0xd12,  0xd2a,  0xd3e,  0xd41,  0xd46,  0xd4a,  0xd4d,  0xd57,  0xd60,  0xd66,  0xd82,
    0xd85,  0xd9a,  0xdb3,  0xdbd,  0xdc0,  0xdca,  0xdcf,  0xdd2,  0xdd6,  0xdd8,  0xdf2,  0xdf4,  0xe01,  0xe31,
    0xe33,  0xe35,  0xe3f,  0xe41,  0xe46,  0xe48,  0xe4f,  0xe51,  0xe5a,  0xe81,  0xe84,  0xe87,  0xe8a,  0xe8d,
    0xe94,  0xe99,  0xea1,  0xea5,  0xea7,  0xeaa,  0xead,  0xeb1,  0xeb3,  0xeb5,  0xebb,  0xebd,  0xec0,  0xec6,
    0xec8,  0xed0,  0xedc,  0xf00,  0xf02,  0xf04,  0xf13,  0xf18,  0xf1a,  0xf20,  0xf2a,  0xf34,  0xf3a,  0xf3e,
    0xf40,  0xf49,  0xf71,  0xf7f,  0xf81,  0xf85,  0xf87,  0xf89,  0xf90,  0xf99,  0xfbe,  0xfc6,  0xfc8,  0xfcf,
    0x1000, 0x1023, 0x1029, 0x102c, 0x102e, 0x1031, 0x1036, 0x1038, 0x1040, 0x104a, 0x1050, 0x1056, 0x1058, 0x10a0,
    0x10d0, 0x10fb, 0x1100, 0x115f, 0x11a8, 0x1200, 0x1208, 0x1248, 0x124a, 0x1250, 0x1258, 0x125a, 0x1260, 0x1288,
    0x128a, 0x1290, 0x12b0, 0x12b2, 0x12b8, 0x12c0, 0x12c2, 0x12c8, 0x12d0, 0x12d8, 0x12f0, 0x1310, 0x1312, 0x1318,
    0x1320, 0x1348, 0x1361, 0x1369, 0x1372, 0x13a0, 0x1401, 0x166d, 0x166f, 0x1680, 0x1682, 0x169b, 0x16a0, 0x16eb,
    0x16ee, 0x1780, 0x17b4, 0x17b7, 0x17be, 0x17c6, 0x17c8, 0x17ca, 0x17d4, 0x17db, 0x17e0, 0x1800, 0x1806, 0x1808,
    0x180b, 0x1810, 0x1820, 0x1843, 0x1845, 0x1880, 0x18a9, 0x1e00, 0x1e96, 0x1ea0, 0x1f00, 0x1f08, 0x1f10, 0x1f18,
    0x1f20, 0x1f28, 0x1f30, 0x1f38, 0x1f40, 0x1f48, 0x1f50, 0x1f59, 0x1f5b, 0x1f5d, 0x1f5f, 0x1f61, 0x1f68, 0x1f70,
    0x1f80, 0x1f88, 0x1f90, 0x1f98, 0x1fa0, 0x1fa8, 0x1fb0, 0x1fb6, 0x1fb8, 0x1fbc, 0x1fbe, 0x1fc0, 0x1fc2, 0x1fc6,
    0x1fc8, 0x1fcc, 0x1fce, 0x1fd0, 0x1fd6, 0x1fd8, 0x1fdd, 0x1fe0, 0x1fe8, 0x1fed, 0x1ff2, 0x1ff6, 0x1ff8, 0x1ffc,
    0x1ffe, 0x2000, 0x200c, 0x2010, 0x2016, 0x2018, 0x201a, 0x201c, 0x201e, 0x2020, 0x2028, 0x202a, 0x202f, 0x2031,
    0x2039, 0x203b, 0x203f, 0x2041, 0x2044, 0x2046, 0x2048, 0x206a, 0x2070, 0x2074, 0x207a, 0x207d, 0x207f, 0x2081,
    0x208a, 0x208d, 0x20a0, 0x20d0, 0x20dd, 0x20e1, 0x20e3, 0x2100, 0x2102, 0x2104, 0x2107, 0x2109, 0x210b, 0x210e,
    0x2110, 0x2113, 0x2115, 0x2117, 0x2119, 0x211e, 0x2124, 0x212b, 0x212e, 0x2130, 0x2132, 0x2134, 0x2136, 0x2139,
    0x2153, 0x2160, 0x2190, 0x2195, 0x219a, 0x219c, 0x21a0, 0x21a2, 0x21a5, 0x21a8, 0x21ae, 0x21b0, 0x21ce, 0x21d0,
    0x21d2, 0x21d6, 0x2200, 0x2300, 0x2308, 0x230c, 0x2320, 0x2322, 0x2329, 0x232b, 0x237d, 0x2400, 0x2440, 0x2460,
    0x249c, 0x24ea, 0x2500, 0x25a0, 0x25b7, 0x25b9, 0x25c1, 0x25c3, 0x2600, 0x2619, 0x266f, 0x2671, 0x2701, 0x2706,
    0x270c, 0x2729, 0x274d, 0x274f, 0x2756, 0x2758, 0x2761, 0x2776, 0x2794, 0x2798, 0x27b1, 0x2800, 0x2e80, 0x2e9b,
    0x2f00, 0x2ff0, 0x3000, 0x3002, 0x3004, 0x3006, 0x3008, 0x3012, 0x3014, 0x301c, 0x301e, 0x3020, 0x3022, 0x302a,
    0x3030, 0x3032, 0x3036, 0x3038, 0x303e, 0x3041, 0x3099, 0x309b, 0x309d, 0x30a1, 0x30fb, 0x30fd, 0x3105, 0x3131,
    0x3190, 0x3192, 0x3196, 0x31a0, 0x3200, 0x3220, 0x322a, 0x3260, 0x327f, 0x3281, 0x328a, 0x32c0, 0x32d0, 0x3300,
    0x337b, 0x33e0, 0x3400, 0x4e00, 0xa000, 0xa490, 0xa4a4, 0xa4b5, 0xa4c2, 0xa4c6, 0xac00, 0xd800, 0xe000, 0xf900,
    0xfb00, 0xfb13, 0xfb1d, 0xfb20, 0xfb29, 0xfb2b, 0xfb38, 0xfb3e, 0xfb40, 0xfb43, 0xfb46, 0xfbd3, 0xfd3e, 0xfd50,
    0xfd92, 0xfdf0, 0xfe20, 0xfe30, 0xfe32, 0xfe34, 0xfe36, 0xfe49, 0xfe4d, 0xfe50, 0xfe54, 0xfe58, 0xfe5a, 0xfe5f,
    0xfe62, 0xfe65, 0xfe68, 0xfe6b, 0xfe70, 0xfe74, 0xfe76, 0xfeff, 0xff01, 0xff04, 0xff06, 0xff08, 0xff0a, 0xff0d,
    0xff0f, 0xff11, 0xff1a, 0xff1c, 0xff1f, 0xff21, 0xff3b, 0xff3d, 0xff3f, 0xff41, 0xff5b, 0xff5d, 0xff61, 0xff63,
    0xff65, 0xff67, 0xff70, 0xff72, 0xff9e, 0xffa0, 0xffc2, 0xffca, 0xffd2, 0xffda, 0xffe0, 0xffe2, 0xffe4, 0xffe6,
    0xffe8, 0xffea, 0xffed, 0xfff9, 0xfffc};

constexpr KChar typeValues[] = {
    0x1f,   0xf,    0x21,   0x180c, 0x23,   0x18,   0x25,   0x181a, 0x27,   0x18,   0x29,   0x1615, 0x2c,   0x1918,
    0x2e,   0x1418, 0x30,   0x1809, 0x39,   0x9,    0x3b,   0x18,   0x3e,   0x19,   0x40,   0x18,   0x5a,   0x1,
    0x5c,   0x1518, 0x5e,   0x161b, 0x60,   0x171b, 0x7a,   0x2,    0x7c,   0x1519, 0x7e,   0x1619, 0x9f,   0xf,
    0xa1,   0x180c, 0xa5,   0x1a,   0xa7,   0x1c,   0xa9,   0x1c1b, 0xab,   0x1d02, 0xad,   0x1419, 0xb0,   0x1b1c,
    0xb2,   0x190b, 0xb4,   0xb1b,  0xb6,   0x21c,  0xb8,   0x181b, 0xba,   0xb02,  0xbc,   0x1e0b, 0xbe,   0xb,
    0xc0,   0x1801, 0xd6,   0x1,    0xd8,   0x1901, 0xde,   0x1,    0xf6,   0x2,    0xf8,   0x1902, 0xff,   0x2,
    0x137,  0x201,  0x148,  0x102,  0x178,  0x201,  0x17e,  0x102,  0x180,  0x2,    0x182,  0x1,    0x186,  0x201,
    0x189,  0x102,  0x18b,  0x1,    0x18d,  0x2,    0x191,  0x1,    0x193,  0x102,  0x196,  0x201,  0x198,  0x1,
    0x19b,  0x2,    0x19d,  0x1,    0x19f,  0x102,  0x1a6,  0x201,  0x1aa,  0x102,  0x1ae,  0x201,  0x1b1,  0x102,
    0x1b3,  0x1,    0x1b7,  0x102,  0x1b9,  0x201,  0x1bb,  0x502,  0x1bd,  0x201,  0x1bf,  0x2,    0x1c3,  0x5,
    0x1c5,  0x301,  0x1c7,  0x102,  0x1c9,  0x203,  0x1cb,  0x301,  0x1dc,  0x102,  0x1ef,  0x201,  0x1f1,  0x102,
    0x1f3,  0x203,  0x1f6,  0x201,  0x1f8,  0x1,    0x21f,  0x201,  0x233,  0x201,  0x2ad,  0x2,    0x2b8,  0x4,
    0x2ba,  0x1b,   0x2c1,  0x4,    0x2cf,  0x1b,   0x2d1,  0x4,    0x2df,  0x1b,   0x2e4,  0x4,    0x2ed,  0x1b,
    0x2ee,  0x4,    0x34e,  0x6,    0x362,  0x6,    0x375,  0x1b,   0x37a,  0x4,    0x37e,  0x18,   0x385,  0x1b,
    0x388,  0x1801, 0x38a,  0x1,    0x38c,  0x1,    0x38f,  0x1,    0x391,  0x102,  0x3a1,  0x1,    0x3ab,  0x1,
    0x3ce,  0x2,    0x3d1,  0x2,    0x3d4,  0x1,    0x3d7,  0x2,    0x3ef,  0x201,  0x3f3,  0x2,    0x42f,  0x1,
    0x45f,  0x2,    0x481,  0x201,  0x483,  0x61c,  0x486,  0x6,    0x489,  0x7,    0x4c0,  0x201,  0x4c4,  0x102,
    0x4c8,  0x102,  0x4cc,  0x102,  0x4f5,  0x201,  0x4f9,  0x201,  0x556,  0x1,    0x55a,  0x418,  0x55f,  0x18,
    0x587,  0x2,    0x58a,  0x1814, 0x5a1,  0x6,    0x5b9,  0x6,    0x5bd,  0x6,    0x5c1,  0x618,  0x5c4,  0x1806,
    0x5ea,  0x5,    0x5f2,  0x5,    0x5f4,  0x18,   0x60c,  0x18,   0x61b,  0x1800, 0x61f,  0x1800, 0x63a,  0x5,
    0x641,  0x504,  0x64a,  0x5,    0x655,  0x6,    0x669,  0x9,    0x66d,  0x18,   0x671,  0x506,  0x6d3,  0x5,
    0x6d5,  0x518,  0x6dc,  0x6,    0x6de,  0x7,    0x6e4,  0x6,    0x6e6,  0x4,    0x6e8,  0x6,    0x6ea,  0x1c06,
    0x6ed,  0x6,    0x6f9,  0x9,    0x6fc,  0x5,    0x6fe,  0x1c,   0x70d,  0x18,   0x710,  0x1005, 0x712,  0x605,
    0x72c,  0x5,    0x74a,  0x6,    0x7a5,  0x5,    0x7b0,  0x6,    0x902,  0x6,    0x903,  0x800,  0x939,  0x5,
    0x93d,  0x506,  0x940,  0x8,    0x948,  0x6,    0x94c,  0x8,    0x94d,  0x600,  0x951,  0x605,  0x954,  0x6,
    0x961,  0x5,    0x963,  0x6,    0x965,  0x18,   0x96f,  0x9,    0x970,  0x18,   0x982,  0x608,  0x983,  0x800,
    0x98c,  0x5,    0x990,  0x5,    0x9a8,  0x5,    0x9b0,  0x5,    0x9b2,  0x5,    0x9b9,  0x5,    0x9bc,  0x6,
    0x9c0,  0x8,    0x9c4,  0x6,    0x9c8,  0x8,    0x9cc,  0x8,    0x9cd,  0x600,  0x9d7,  0x800,  0x9dd,  0x5,
    0x9e1,  0x5,    0x9e3,  0x6,    0x9ef,  0x9,    0x9f1,  0x5,    0x9f3,  0x1a,   0x9f9,  0xb,    0x9fa,  0x1c,
    0xa02,  0x6,    0xa0a,  0x5,    0xa10,  0x5,    0xa28,  0x5,    0xa30,  0x5,    0xa33,  0x5,    0xa36,  0x5,
    0xa39,  0x5,    0xa3c,  0x6,    0xa40,  0x8,    0xa42,  0x6,    0xa48,  0x6,    0xa4d,  0x6,    0xa5c,  0x5,
    0xa5e,  0x5,    0xa6f,  0x9,    0xa71,  0x6,    0xa74,  0x5,    0xa82,  0x6,    0xa83,  0x800,  0xa8b,  0x5,
    0xa8d,  0x500,  0xa91,  0x5,    0xaa8,  0x5,    0xab0,  0x5,    0xab3,  0x5,    0xab9,  0x5,    0xabd,  0x506,
    0xac0,  0x8,    0xac5,  0x6,    0xac8,  0x6,    0xac9,  0x800,  0xacc,  0x8,    0xacd,  0x600,  0xad0,  0x5,
    0xae0,  0x5,    0xaef,  0x9,    0xb02,  0x608,  0xb03,  0x800,  0xb0c,  0x5,    0xb10,  0x5,    0xb28,  0x5,
    0xb30,  0x5,    0xb33,  0x5,    0xb39,  0x5,    0xb3d,  0x506,  0xb41,  0x608,  0xb43,  0x6,    0xb48,  0x8,
    0xb4c,  0x8,    0xb4d,  0x600,  0xb57,  0x806,  0xb5d,  0x5,    0xb61,  0x5,    0xb6f,  0x9,    0xb70,  0x1c,
    0xb83,  0x806,  0xb8a,  0x5,    0xb90,  0x5,    0xb95,  0x5,    0xb9a,  0x5,    0xb9c,  0x5,    0xb9f,  0x5,
    0xba4,  0x5,    0xbaa,  0x5,    0xbb5,  0x5,    0xbb9,  0x5,    0xbbf,  0x8,    0xbc1,  0x806,  0xbc2,  0x8,
    0xbc8,  0x8,    0xbcc,  0x8,    0xbcd,  0x600,  0xbd7,  0x800,  0xbef,  0x9,    0xbf2,  0xb,    0xc03,  0x8,
    0xc0c,  0x5,    0xc10,  0x5,    0xc28,  0x5,    0xc33,  0x5,    0xc39,  0x5,    0xc40,  0x6,    0xc44,  0x8,
    0xc48,  0x6,    0xc4d,  0x6,    0xc56,  0x6,    0xc61,  0x5,    0xc6f,  0x9,    0xc83,  0x8,    0xc8c,  0x5,
    0xc90,  0x5,    0xca8,  0x5,    0xcb3,  0x5,    0xcb9,  0x5,    0xcc0,  0x608,  0xcc4,  0x8,    0xcc7,  0x806,
    0xcc8,  0x8,    0xccb,  0x8,    0xccd,  0x6,    0xcd6,  0x8,    0xcde,  0x5,    0xce1,  0x5,    0xcef,  0x9,
    0xd03,  0x8,    0xd0c,  0x5,    0xd10,  0x5,    0xd28,  0x5,    0xd39,  0x5,    0xd40,  0x8,    0xd43,  0x6,
    0xd48,  0x8,    0xd4c,  0x8,    0xd4d,  0x600,  0xd57,  0x800,  0xd61,  0x5,    0xd6f,  0x9,    0xd83,  0x8,
    0xd96,  0x5,    0xdb1,  0x5,    0xdbb,  0x5,    0xdbd,  0x500,  0xdc6,  0x5,    0xdca,  0x6,    0xdd1,  0x8,
    0xdd4,  0x6,    0xdd6,  0x6,    0xddf,  0x8,    0xdf3,  0x8,    0xdf4,  0x18,   0xe30,  0x5,    0xe32,  0x605,
    0xe34,  0x506,  0xe3a,  0x6,    0xe40,  0x1a05, 0xe45,  0x5,    0xe47,  0x604,  0xe4e,  0x6,    0xe50,  0x1809,
    0xe59,  0x9,    0xe5b,  0x18,   0xe82,  0x5,    0xe84,  0x5,    0xe88,  0x5,    0xe8a,  0x5,    0xe8d,  0x500,
    0xe97,  0x5,    0xe9f,  0x5,    0xea3,  0x5,    0xea5,  0x500,  0xea7,  0x500,  0xeab,  0x5,    0xeb0,  0x5,
    0xeb2,  0x605,  0xeb4,  0x506,  0xeb9,  0x6,    0xebc,  0x6,    0xebd,  0x500,  0xec4,  0x5,    0xec6,  0x4,
    0xecd,  0x6,    0xed9,  0x9,    0xedd,  0x5,    0xf01,  0x1c05, 0xf03,  0x1c,   0xf12,  0x18,   0xf17,  0x1c,
    0xf19,  0x6,    0xf1f,  0x1c,   0xf29,  0x9,    0xf33,  0xb,    0xf39,  0x61c,  0xf3d,  0x1615, 0xf3f,  0x8,
    0xf47,  0x5,    0xf6a,  0x5,    0xf7e,  0x6,    0xf80,  0x806,  0xf84,  0x6,    0xf86,  0x1806, 0xf88,  0x605,
    0xf8b,  0x5,    0xf97,  0x6,    0xfbc,  0x6,    0xfc5,  0x1c,   0xfc7,  0x1c06, 0xfcc,  0x1c,   0xfcf,  0x1c00,
    0x1021, 0x5,    0x1027, 0x5,    0x102a, 0x5,    0x102d, 0x608,  0x1030, 0x6,    0x1032, 0x806,  0x1037, 0x6,
    0x1039, 0x608,  0x1049, 0x9,    0x104f, 0x18,   0x1055, 0x5,    0x1057, 0x8,    0x1059, 0x6,    0x10c5, 0x1,
    0x10f6, 0x5,    0x10fb, 0x1800, 0x1159, 0x5,    0x11a2, 0x5,    0x11f9, 0x5,    0x1206, 0x5,    0x1246, 0x5,
    0x1248, 0x5,    0x124d, 0x5,    0x1256, 0x5,    0x1258, 0x5,    0x125d, 0x5,    0x1286, 0x5,    0x1288, 0x5,
    0x128d, 0x5,    0x12ae, 0x5,    0x12b0, 0x5,    0x12b5, 0x5,    0x12be, 0x5,    0x12c0, 0x5,    0x12c5, 0x5,
    0x12ce, 0x5,    0x12d6, 0x5,    0x12ee, 0x5,    0x130e, 0x5,    0x1310, 0x5,    0x1315, 0x5,    0x131e, 0x5,
    0x1346, 0x5,    0x135a, 0x5,    0x1368, 0x18,   0x1371, 0x9,    0x137c, 0xb,    0x13f4, 0x5,    0x166c, 0x5,
    0x166e, 0x18,   0x1676, 0x5,    0x1681, 0x50c,  0x169a, 0x5,    0x169c, 0x1516, 0x16ea, 0x5,    0x16ed, 0x18,
    0x16f0, 0xb,    0x17b3, 0x5,    0x17b6, 0x8,    0x17bd, 0x6,    0x17c5, 0x8,    0x17c7, 0x806,  0x17c9, 0x608,
    0x17d3, 0x6,    0x17da, 0x18,   0x17dc, 0x1a18, 0x17e9, 0x9,    0x1805, 0x18,   0x1807, 0x1814, 0x180a, 0x18,
    0x180e, 0x10,   0x1819, 0x9,    0x1842, 0x5,    0x1844, 0x405,  0x1877, 0x5,    0x18a8, 0x5,    0x18a9, 0x600,
    0x1e95, 0x201,  0x1e9b, 0x2,    0x1ef9, 0x201,  0x1f07, 0x2,    0x1f0f, 0x1,    0x1f15, 0x2,    0x1f1d, 0x1,
    0x1f27, 0x2,    0x1f2f, 0x1,    0x1f37, 0x2,    0x1f3f, 0x1,    0x1f45, 0x2,    0x1f4d, 0x1,    0x1f57, 0x2,
    0x1f59, 0x100,  0x1f5b, 0x100,  0x1f5d, 0x100,  0x1f60, 0x102,  0x1f67, 0x2,    0x1f6f, 0x1,    0x1f7d, 0x2,
    0x1f87, 0x2,    0x1f8f, 0x3,    0x1f97, 0x2,    0x1f9f, 0x3,    0x1fa7, 0x2,    0x1faf, 0x3,    0x1fb4, 0x2,
    0x1fb7, 0x2,    0x1fbb, 0x1,    0x1fbd, 0x1b03, 0x1fbf, 0x1b02, 0x1fc1, 0x1b,   0x1fc4, 0x2,    0x1fc7, 0x2,
    0x1fcb, 0x1,    0x1fcd, 0x1b03, 0x1fcf, 0x1b,   0x1fd3, 0x2,    0x1fd7, 0x2,    0x1fdb, 0x1,    0x1fdf, 0x1b,
    0x1fe7, 0x2,    0x1fec, 0x1,    0x1fef, 0x1b,   0x1ff4, 0x2,    0x1ff7, 0x2,    0x1ffb, 0x1,    0x1ffd, 0x1b03,
    0x1ffe, 0x1b,   0x200b, 0xc,    0x200f, 0x10,   0x2015, 0x14,   0x2017, 0x18,   0x2019, 0x1e1d, 0x201b, 0x1d15,
    0x201d, 0x1e1d, 0x201f, 0x1d15, 0x2027, 0x18,   0x2029, 0xe0d,  0x202e, 0x10,   0x2030, 0xc18,  0x2038, 0x18,
    0x203a, 0x1d1e, 0x203e, 0x18,   0x2040, 0x17,   0x2043, 0x18,   0x2045, 0x1519, 0x2046, 0x16,   0x204d, 0x18,
    0x206f, 0x10,   0x2070, 0xb,    0x2079, 0xb,    0x207c, 0x19,   0x207e, 0x1516, 0x2080, 0x20b,  0x2089, 0xb,
    0x208c, 0x19,   0x208e, 0x1516, 0x20af, 0x1a,   0x20dc, 0x6,    0x20e0, 0x7,    0x20e2, 0x607,  0x20e3, 0x700,
    0x2101, 0x1c,   0x2103, 0x1c01, 0x2106, 0x1c,   0x2108, 0x11c,  0x210a, 0x1c02, 0x210d, 0x1,    0x210f, 0x2,
    0x2112, 0x1,    0x2114, 0x21c,  0x2116, 0x11c,  0x2118, 0x1c,   0x211d, 0x1,    0x2123, 0x1c,   0x212a, 0x1c01,
    0x212d, 0x1,    0x212f, 0x21c,  0x2131, 0x1,    0x2133, 0x11c,  0x2135, 0x502,  0x2138, 0x5,    0x213a, 0x21c,
    0x215f, 0xb,    0x2183, 0xa,    0x2194, 0x19,   0x2199, 0x1c,   0x219b, 0x19,   0x219f, 0x1c,   0x21a1, 0x1c19,
    0x21a4, 0x191c, 0x21a7, 0x1c19, 0x21ad, 0x1c,   0x21af, 0x1c19, 0x21cd, 0x1c,   0x21cf, 0x19,   0x21d1, 0x1c,
    0x21d5, 0x1c19, 0x21f3, 0x1c,   0x22f1, 0x19,   0x2307, 0x1c,   0x230b, 0x19,   0x231f, 0x1c,   0x2321, 0x19,
    0x2328, 0x1c,   0x232a, 0x1516, 0x237b, 0x1c,   0x239a, 0x1c,   0x2426, 0x1c,   0x244a, 0x1c,   0x249b, 0xb,
    0x24e9, 0x1c,   0x24ea, 0xb,    0x2595, 0x1c,   0x25b6, 0x1c,   0x25b8, 0x191c, 0x25c0, 0x1c,   0x25c2, 0x191c,
    0x25f7, 0x1c,   0x2613, 0x1c,   0x266e, 0x1c,   0x2670, 0x191c, 0x2671, 0x1c00, 0x2704, 0x1c,   0x2709, 0x1c,
    0x2727, 0x1c,   0x274b, 0x1c,   0x274d, 0x1c00, 0x2752, 0x1c,   0x2756, 0x1c,   0x275e, 0x1c,   0x2767, 0x1c,
    0x2793, 0xb,    0x2794, 0x1c,   0x27af, 0x1c,   0x27be, 0x1c,   0x28ff, 0x1c,   0x2e99, 0x1c,   0x2ef3, 0x1c,
    0x2fd5, 0x1c,   0x2ffb, 0x1c,   0x3001, 0x180c, 0x3003, 0x18,   0x3005, 0x41c,  0x3007, 0xa05,  0x3011, 0x1615,
    0x3013, 0x1c,   0x301b, 0x1615, 0x301d, 0x1514, 0x301f, 0x16,   0x3021, 0xa1c,  0x3029, 0xa,    0x302f, 0x6,
    0x3031, 0x414,  0x3035, 0x4,    0x3037, 0x1c,   0x303a, 0xa,    0x303f, 0x1c,   0x3094, 0x5,    0x309a, 0x6,
    0x309c, 0x1b,   0x309e, 0x4,    0x30fa, 0x5,    0x30fc, 0x1704, 0x30fe, 0x4,    0x312c, 0x5,    0x318e, 0x5,
    0x3191, 0x1c,   0x3195, 0xb,    0x319f, 0x1c,   0x31b7, 0x5,    0x321c, 0x1c,   0x3229, 0xb,    0x3243, 0x1c,
    0x327b, 0x1c,   0x3280, 0x1c0b, 0x3289, 0xb,    0x32b0, 0x1c,   0x32cb, 0x1c,   0x32fe, 0x1c,   0x3376, 0x1c,
    0x33dd, 0x1c,   0x33fe, 0x1c,   0x4db5, 0x5,    0x9fa5, 0x5,    0xa48c, 0x5,    0xa4a1, 0x1c,   0xa4b3, 0x1c,
    0xa4c0, 0x1c,   0xa4c4, 0x1c,   0xa4c6, 0x1c,   0xd7a3, 0x5,    0xdfff, 0x13,   0xf8ff, 0x12,   0xfa2d, 0x5,
    0xfb06, 0x2,    0xfb17, 0x2,    0xfb1f, 0x506,  0xfb28, 0x5,    0xfb2a, 0x1905, 0xfb36, 0x5,    0xfb3c, 0x5,
    0xfb3e, 0x5,    0xfb41, 0x5,    0xfb44, 0x5,    0xfbb1, 0x5,    0xfd3d, 0x5,    0xfd3f, 0x1615, 0xfd8f, 0x5,
    0xfdc7, 0x5,    0xfdfb, 0x5,    0xfe23, 0x6,    0xfe31, 0x1418, 0xfe33, 0x1714, 0xfe35, 0x1517, 0xfe44, 0x1516,
    0xfe4c, 0x18,   0xfe4f, 0x17,   0xfe52, 0x18,   0xfe57, 0x18,   0xfe59, 0x1514, 0xfe5e, 0x1516, 0xfe61, 0x18,
    0xfe64, 0x1419, 0xfe66, 0x19,   0xfe6a, 0x1a18, 0xfe6b, 0x1800, 0xfe72, 0x5,    0xfe74, 0x5,    0xfefc, 0x5,
    0xfeff, 0x1000, 0xff03, 0x18,   0xff05, 0x181a, 0xff07, 0x18,   0xff09, 0x1615, 0xff0c, 0x1918, 0xff0e, 0x1418,
    0xff10, 0x1809, 0xff19, 0x9,    0xff1b, 0x18,   0xff1e, 0x19,   0xff20, 0x18,   0xff3a, 0x1,    0xff3c, 0x1518,
    0xff3e, 0x161b, 0xff40, 0x171b, 0xff5a, 0x2,    0xff5c, 0x1519, 0xff5e, 0x1619, 0xff62, 0x1815, 0xff64, 0x1618,
    0xff66, 0x1705, 0xff6f, 0x5,    0xff71, 0x504,  0xff9d, 0x5,    0xff9f, 0x4,    0xffbe, 0x5,    0xffc7, 0x5,
    0xffcf, 0x5,    0xffd7, 0x5,    0xffdc, 0x5,    0xffe1, 0x1a,   0xffe3, 0x1b19, 0xffe5, 0x1a1c, 0xffe6, 0x1a,
    0xffe9, 0x191c, 0xffec, 0x19,   0xffee, 0x1c,   0xfffb, 0x10};

KInt getType(KChar ch) {
    if (ch < 1000) {
        return typeValuesCache[ch];
    }
    int result = binarySearchRange(typeKeys, ARRAY_SIZE(typeKeys), ch);
    int high = typeValues[result * 2];
    if (ch <= high) {
        int code = typeValues[result * 2 + 1];
        if (code < 0x100) {
            return code;
        }
        return (ch & 1) == 1 ? code >> 8 : code & 0xff;
    }
    return UNASSIGNED;
}

KChar towupper_Konan(KChar ch) {
    // Optimized case for ASCII.
    if ('a' <= ch && ch <= 'z') {
        return ch - ('a' - 'A');
    }
    if (ch < 181) {
        return ch;
    }
    if (ch < 1000) {
        return uppercaseValuesCache[ch - 181];
    }
    int result = binarySearchRange(uppercaseKeys, ARRAY_SIZE(uppercaseKeys), ch);
    if (result >= 0) {
        bool by2 = false;
        KChar start = uppercaseKeys[result];
        KChar end = uppercaseValues[result * 2];
        if ((start & 0x8000) != (end & 0x8000)) {
            end ^= 0x8000;
            by2 = true;
        }
        if (ch <= end) {
            if (by2 && (ch & 1) != (start & 1)) {
                return ch;
            }
            KChar mapping = uppercaseValues[result * 2 + 1];
            return ch + mapping;
        }
    }
    return ch;
}

KChar towlower_Konan(KChar ch) {
    // Optimized case for ASCII.
    if ('A' <= ch && ch <= 'Z') {
        return ch + ('a' - 'A');
    }
    if (ch < 192) {
        return ch;
    }
    if (ch < 1000) {
        return lowercaseValuesCache[ch - 192];
    }

    int result = binarySearchRange(lowercaseKeys, ARRAY_SIZE(lowercaseKeys), ch);
    if (result >= 0) {
        bool by2 = false;
        KChar start = lowercaseKeys[result];
        KChar end = lowercaseValues[result * 2];
        if ((start & 0x8000) != (end & 0x8000)) {
            end ^= 0x8000;
            by2 = true;
        }
        if (ch <= end) {
            if (by2 && (ch & 1) != (start & 1)) {
                return ch;
            }
            KChar mapping = lowercaseValues[result * 2 + 1];
            return ch + mapping;
        }
    }
    return ch;
}

int iswdigit_Konan(KChar ch) {
    // Optimized case for ASCII.
    if ('0' <= ch && ch <= '9') {
        return true;
    }
    if (ch < 1632) {
        return false;
    }
    return getType(ch) == DECIMAL_DIGIT_NUMBER;
}

int iswalpha_Konan(KChar ch) {
    if (('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z')) {
        return true;
    }
    if (ch < 128) {
        return false;
    }
    int type = getType(ch);
    return type >= UPPERCASE_LETTER && type <= OTHER_LETTER;
}

int iswalnum_Konan(KChar ch) {
    int type = getType(ch);
    return (type >= UPPERCASE_LETTER && type <= OTHER_LETTER) || type == DECIMAL_DIGIT_NUMBER;
}

int iswspace_Konan(KChar ch) {
    // FILE, GROUP, RECORD, UNIT separators, # Zs SPACE or # Cc CONTROLs
    if ((ch >= 0x1c && ch <= 0x20) || (ch >= 0x9 && ch <= 0xd)) {
        return true;
    }
    // not (# Zs OGHAM SPACE MARK or # Cc or # Zs NO-BREAK SPACE)
    if (ch < 0x2000 && !(ch == 0x1680 || ch == 0x85 || ch == 0xA0)) {
        return false;
    }
    // if # Zl LINE SEPARATOR or # Zp PARAGRAPH SEPARATOR or # Zs NARROW NO-BREAK SPACE
    // or # Zs MEDIUM MATHEMATICAL SPACE or # Zs IDEOGRAPHIC SPACE
    return ch < 0x200b || ch == 0x2028 || ch == 0x2029 || ch == 0x202F || ch == 0x205F || ch == 0x3000;
}

int iswupper_Konan(KChar ch) {
    // Optimized case for ASCII.
    if ('A' <= ch && ch <= 'Z') {
        return true;
    }
    if (ch < 128) {
        return false;
    }

    return getType(ch) == UPPERCASE_LETTER;
}

int iswlower_Konan(KChar ch) {
    // Optimized case for ASCII.
    if ('a' <= ch && ch <= 'z') {
        return true;
    }
    if (ch < 128) {
        return false;
    }

    return getType(ch) == LOWERCASE_LETTER;
}

} // namespace

extern "C" {

OBJ_GETTER(CreateStringFromCString, const char* cstring) {
    RETURN_RESULT_OF(utf8ToUtf16, cstring, cstring ? strlen(cstring) : 0);
}

OBJ_GETTER(CreateStringFromUtf8, const char* utf8, uint32_t lengthBytes) {
    RETURN_RESULT_OF(utf8ToUtf16, utf8, lengthBytes);
}

char* CreateCStringFromString(KConstRef kref) {
    if (kref == nullptr)
        return nullptr;
    KString kstring = kref->array();
    const KChar* utf16 = CharArrayAddressOfElementAt(kstring, 0);
    KStdString utf8;
    utf8.reserve(kstring->count_);
    utf8::unchecked::utf16to8(utf16, utf16 + kstring->count_, back_inserter(utf8));
    char* result = reinterpret_cast<char*>(konan::calloc(1, utf8.size() + 1));
    ::memcpy(result, utf8.c_str(), utf8.size());
    return result;
}

void DisposeCString(char* cstring) {
    if (cstring)
        konan::free(cstring);
}

// String.kt
OBJ_GETTER(Kotlin_String_replace, KString thiz, KChar oldChar, KChar newChar, KBoolean ignoreCase) {
    auto count = thiz->count_;
    ArrayHeader* result = AllocArrayInstance(theStringTypeInfo, count, OBJ_RESULT)->array();
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, 0);
    KChar* resultRaw = CharArrayAddressOfElementAt(result, 0);
    if (ignoreCase) {
        KChar oldCharLower = towlower_Konan(oldChar);
        for (KInt index = 0; index < count; ++index) {
            KChar thizChar = *thizRaw++;
            *resultRaw++ = towlower_Konan(thizChar) == oldCharLower ? newChar : thizChar;
        }
    } else {
        for (KInt index = 0; index < count; ++index) {
            KChar thizChar = *thizRaw++;
            *resultRaw++ = thizChar == oldChar ? newChar : thizChar;
        }
    }
    RETURN_OBJ(result->obj());
}

OBJ_GETTER(Kotlin_String_plusImpl, KString thiz, KString other) {
    RuntimeAssert(thiz != nullptr, "this cannot be null");
    RuntimeAssert(other != nullptr, "other cannot be null");
    RuntimeAssert(thiz->type_info() == theStringTypeInfo, "Must be a string");
    RuntimeAssert(other->type_info() == theStringTypeInfo, "Must be a string");
    KInt result_length = thiz->count_ + other->count_;
    if (result_length < thiz->count_ || result_length < other->count_) {
        ThrowArrayIndexOutOfBoundsException();
    }
    ArrayHeader* result = AllocArrayInstance(theStringTypeInfo, result_length, OBJ_RESULT)->array();
    memcpy(CharArrayAddressOfElementAt(result, 0), CharArrayAddressOfElementAt(thiz, 0), thiz->count_ * sizeof(KChar));
    memcpy(
        CharArrayAddressOfElementAt(result, thiz->count_), CharArrayAddressOfElementAt(other, 0),
        other->count_ * sizeof(KChar));
    RETURN_OBJ(result->obj());
}

OBJ_GETTER(Kotlin_String_toUpperCase, KString thiz) {
    auto count = thiz->count_;
    ArrayHeader* result = AllocArrayInstance(theStringTypeInfo, count, OBJ_RESULT)->array();
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, 0);
    KChar* resultRaw = CharArrayAddressOfElementAt(result, 0);
    for (KInt index = 0; index < count; ++index) {
        *resultRaw++ = towupper_Konan(*thizRaw++);
    }
    RETURN_OBJ(result->obj());
}

OBJ_GETTER(Kotlin_String_toLowerCase, KString thiz) {
    auto count = thiz->count_;
    ArrayHeader* result = AllocArrayInstance(theStringTypeInfo, count, OBJ_RESULT)->array();
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, 0);
    KChar* resultRaw = CharArrayAddressOfElementAt(result, 0);
    for (KInt index = 0; index < count; ++index) {
        *resultRaw++ = towlower_Konan(*thizRaw++);
    }
    RETURN_OBJ(result->obj());
}

OBJ_GETTER(Kotlin_String_unsafeStringFromCharArray, KConstRef thiz, KInt start, KInt size) {
    const ArrayHeader* array = thiz->array();
    RuntimeAssert(array->type_info() == theCharArrayTypeInfo, "Must use a char array");

    if (size == 0) {
        RETURN_RESULT_OF0(TheEmptyString);
    }

    ArrayHeader* result = AllocArrayInstance(theStringTypeInfo, size, OBJ_RESULT)->array();
    memcpy(CharArrayAddressOfElementAt(result, 0), CharArrayAddressOfElementAt(array, start), size * sizeof(KChar));
    RETURN_OBJ(result->obj());
}

OBJ_GETTER(Kotlin_String_toCharArray, KString string, KInt start, KInt size) {
    ArrayHeader* result = AllocArrayInstance(theCharArrayTypeInfo, size, OBJ_RESULT)->array();
    memcpy(CharArrayAddressOfElementAt(result, 0), CharArrayAddressOfElementAt(string, start), size * sizeof(KChar));
    RETURN_OBJ(result->obj());
}

OBJ_GETTER(Kotlin_String_subSequence, KString thiz, KInt startIndex, KInt endIndex) {
    if (startIndex < 0 || endIndex > thiz->count_ || startIndex > endIndex) {
        // TODO: is it correct exception?
        ThrowArrayIndexOutOfBoundsException();
    }
    if (startIndex == endIndex) {
        RETURN_RESULT_OF0(TheEmptyString);
    }
    KInt length = endIndex - startIndex;
    ArrayHeader* result = AllocArrayInstance(theStringTypeInfo, length, OBJ_RESULT)->array();
    memcpy(
        CharArrayAddressOfElementAt(result, 0), CharArrayAddressOfElementAt(thiz, startIndex), length * sizeof(KChar));
    RETURN_OBJ(result->obj());
}

KInt Kotlin_String_compareTo(KString thiz, KString other) {
    int result = memcmp(
        CharArrayAddressOfElementAt(thiz, 0), CharArrayAddressOfElementAt(other, 0),
        (thiz->count_ < other->count_ ? thiz->count_ : other->count_) * sizeof(KChar));
    if (result != 0)
        return result;
    int diff = thiz->count_ - other->count_;
    if (diff == 0)
        return 0;
    return diff < 0 ? -1 : 1;
}

KInt Kotlin_String_compareToIgnoreCase(KString thiz, KConstRef other) {
    RuntimeAssert(thiz->type_info() == theStringTypeInfo && other->type_info() == theStringTypeInfo, "Must be strings");
    // Important, due to literal internalization.
    KString otherString = other->array();
    if (thiz == otherString)
        return 0;
    auto count = thiz->count_ < otherString->count_ ? thiz->count_ : otherString->count_;
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, 0);
    const KChar* otherRaw = CharArrayAddressOfElementAt(otherString, 0);
    for (KInt index = 0; index < count; ++index) {
        int diff = towlower_Konan(*thizRaw++) - towlower_Konan(*otherRaw++);
        if (diff != 0)
            return diff < 0 ? -1 : 1;
    }
    if (otherString->count_ == thiz->count_)
        return 0;
    else if (otherString->count_ > thiz->count_)
        return -1;
    else
        return 1;
}


KChar Kotlin_String_get(KString thiz, KInt index) {
    if (static_cast<uint32_t>(index) >= thiz->count_) {
        ThrowArrayIndexOutOfBoundsException();
    }
    return *CharArrayAddressOfElementAt(thiz, index);
}

KInt Kotlin_String_getStringLength(KString thiz) {
    return thiz->count_;
}

const char* unsafeByteArrayAsCString(KConstRef thiz, KInt start, KInt size) {
    const ArrayHeader* array = thiz->array();
    RuntimeAssert(array->type_info() == theByteArrayTypeInfo, "Must use a byte array");
    return reinterpret_cast<const char*>(ByteArrayAddressOfElementAt(array, start));
}

OBJ_GETTER(Kotlin_ByteArray_unsafeStringFromUtf8OrThrow, KConstRef thiz, KInt start, KInt size) {
    if (size == 0) {
        RETURN_RESULT_OF0(TheEmptyString);
    }
    const char* rawString = unsafeByteArrayAsCString(thiz, start, size);
    RETURN_RESULT_OF(utf8ToUtf16OrThrow, rawString, size);
}

OBJ_GETTER(Kotlin_ByteArray_unsafeStringFromUtf8, KConstRef thiz, KInt start, KInt size) {
    if (size == 0) {
        RETURN_RESULT_OF0(TheEmptyString);
    }
    const char* rawString = unsafeByteArrayAsCString(thiz, start, size);
    RETURN_RESULT_OF(utf8ToUtf16, rawString, size);
}

OBJ_GETTER(Kotlin_String_unsafeStringToUtf8, KString thiz, KInt start, KInt size) {
    RETURN_RESULT_OF(unsafeUtf16ToUtf8Impl<utf8::with_replacement::utf16to8>, thiz, start, size);
}

OBJ_GETTER(Kotlin_String_unsafeStringToUtf8OrThrow, KString thiz, KInt start, KInt size) {
    RETURN_RESULT_OF(unsafeUtf16ToUtf8Impl<utf16toUtf8OrThrow>, thiz, start, size);
}

KInt Kotlin_StringBuilder_insertString(KRef builder, KInt distIndex, KString fromString, KInt sourceIndex, KInt count) {
    auto toArray = builder->array();
    RuntimeAssert(sourceIndex >= 0 && sourceIndex + count <= fromString->count_, "must be true");
    RuntimeAssert(distIndex >= 0 && distIndex + count <= toArray->count_, "must be true");
    memcpy(
        CharArrayAddressOfElementAt(toArray, distIndex), CharArrayAddressOfElementAt(fromString, sourceIndex),
        count * sizeof(KChar));
    return count;
}

KInt Kotlin_StringBuilder_insertInt(KRef builder, KInt position, KInt value) {
    auto toArray = builder->array();
    RuntimeAssert(toArray->count_ >= 11 + position, "must be true");
    char cstring[12];
    auto length = konan::snprintf(cstring, sizeof(cstring), "%d", value);
    auto* from = &cstring[0];
    auto* to = CharArrayAddressOfElementAt(toArray, position);
    auto* end = from + length;
    while (from != end) {
        *to++ = *from++;
    }
    return length;
}


KBoolean Kotlin_String_equals(KString thiz, KConstRef other) {
    if (other == nullptr || other->type_info() != theStringTypeInfo)
        return false;
    // Important, due to literal internalization.
    KString otherString = other->array();
    if (thiz == otherString)
        return true;
    return thiz->count_ == otherString->count_ &&
        memcmp(
            CharArrayAddressOfElementAt(thiz, 0), CharArrayAddressOfElementAt(otherString, 0),
            thiz->count_ * sizeof(KChar)) == 0;
}

KBoolean Kotlin_String_equalsIgnoreCase(KString thiz, KConstRef other) {
    RuntimeAssert(thiz->type_info() == theStringTypeInfo && other->type_info() == theStringTypeInfo, "Must be strings");
    // Important, due to literal internalization.
    KString otherString = other->array();
    if (thiz == otherString)
        return true;
    if (thiz->count_ != otherString->count_)
        return false;
    auto count = thiz->count_;
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, 0);
    const KChar* otherRaw = CharArrayAddressOfElementAt(otherString, 0);
    for (KInt index = 0; index < count; ++index) {
        if (towlower_Konan(*thizRaw++) != towlower_Konan(*otherRaw++))
            return false;
    }
    return true;
}

KBoolean Kotlin_String_regionMatches(
    KString thiz, KInt thizOffset, KString other, KInt otherOffset, KInt length, KBoolean ignoreCase) {
    if (length < 0 || thizOffset < 0 || length > static_cast<KInt>(thiz->count_) - thizOffset || otherOffset < 0 ||
        length > static_cast<KInt>(other->count_) - otherOffset) {
        return false;
    }
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, thizOffset);
    const KChar* otherRaw = CharArrayAddressOfElementAt(other, otherOffset);
    if (ignoreCase) {
        for (KInt index = 0; index < length; ++index) {
            if (towlower_Konan(*thizRaw++) != towlower_Konan(*otherRaw++))
                return false;
        }
    } else {
        for (KInt index = 0; index < length; ++index) {
            if (*thizRaw++ != *otherRaw++)
                return false;
        }
    }
    return true;
}

KBoolean Kotlin_Char_isDefined(KChar ch) {
    // TODO: fixme!
    RuntimeAssert(false, "Kotlin_Char_isDefined() is not implemented");
    return true;
}

KBoolean Kotlin_Char_isLetter(KChar ch) {
    return iswalpha_Konan(ch) != 0;
}

KBoolean Kotlin_Char_isLetterOrDigit(KChar ch) {
    return iswalnum_Konan(ch) != 0;
}

KBoolean Kotlin_Char_isDigit(KChar ch) {
    return iswdigit_Konan(ch) != 0;
}

KBoolean Kotlin_Char_isIdentifierIgnorable(KChar ch) {
    RuntimeAssert(false, "Kotlin_Char_isIdentifierIgnorable() is not implemented");
    return false;
}

KBoolean Kotlin_Char_isISOControl(KChar ch) {
    return (ch <= 0x1F) || (ch >= 0x7F && ch <= 0x9F);
}

KBoolean Kotlin_Char_isHighSurrogate(KChar ch) {
    return ((ch & 0xfc00) == 0xd800);
}

KBoolean Kotlin_Char_isLowSurrogate(KChar ch) {
    return ((ch & 0xfc00) == 0xdc00);
}

KBoolean Kotlin_Char_isWhitespace(KChar ch) {
    return iswspace_Konan(ch) != 0;
}

KBoolean Kotlin_Char_isLowerCase(KChar ch) {
    return iswlower_Konan(ch) != 0;
}

KBoolean Kotlin_Char_isUpperCase(KChar ch) {
    return iswupper_Konan(ch) != 0;
}

KChar Kotlin_Char_toLowerCase(KChar ch) {
    return towlower_Konan(ch);
}

KChar Kotlin_Char_toUpperCase(KChar ch) {
    return towupper_Konan(ch);
}

KInt Kotlin_Char_getType(KChar ch) {
    return getType(ch);
}

constexpr KInt digits[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, 10, 11,
                           12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                           31, 32, 33, 34, 35, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17,
                           18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

// Based on Apache Harmony implementation.
// Radix check is performed on the Kotlin side.
KInt Kotlin_Char_digitOfChecked(KChar ch, KInt radix) {
    KInt result = -1;
    if (ch >= 0x30 /* 0 */ && ch <= 0x7a /* z */) {
        result = digits[ch - 0x30];
    } else {
        int index = -1;
        index = binarySearchRange(digitKeys, ARRAY_SIZE(digitKeys), ch);
        if (index >= 0 && ch <= digitValues[index * 2]) {
            result = ch - digitValues[index * 2 + 1];
        }
    }
    if (result >= radix)
        return -1;
    return result;
}

KInt Kotlin_String_indexOfChar(KString thiz, KChar ch, KInt fromIndex) {
    if (fromIndex < 0) {
        fromIndex = 0;
    }
    if (fromIndex > thiz->count_) {
        return -1;
    }
    KInt count = thiz->count_;
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, fromIndex);
    while (fromIndex < count) {
        if (*thizRaw++ == ch)
            return fromIndex;
        fromIndex++;
    }
    return -1;
}

KInt Kotlin_String_lastIndexOfChar(KString thiz, KChar ch, KInt fromIndex) {
    if (fromIndex < 0 || thiz->count_ == 0) {
        return -1;
    }
    if (fromIndex >= thiz->count_) {
        fromIndex = thiz->count_ - 1;
    }
    KInt count = thiz->count_;
    KInt index = fromIndex;
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, index);
    while (index >= 0) {
        if (*thizRaw-- == ch)
            return index;
        index--;
    }
    return -1;
}

// TODO: or code up Knuth-Moris-Pratt.
KInt Kotlin_String_indexOfString(KString thiz, KString other, KInt fromIndex) {
    if (fromIndex < 0) {
        fromIndex = 0;
    }
    if (fromIndex >= thiz->count_) {
        return (other->count_ == 0) ? thiz->count_ : -1;
    }
    if (other->count_ > static_cast<KInt>(thiz->count_) - fromIndex) {
        return -1;
    }
    // An empty string can be always found.
    if (other->count_ == 0) {
        return fromIndex;
    }
    KInt count = thiz->count_;
    const KChar* thizRaw = CharArrayAddressOfElementAt(thiz, fromIndex);
    const KChar* otherRaw = CharArrayAddressOfElementAt(other, 0);
    void* result =
        konan::memmem(thizRaw, (thiz->count_ - fromIndex) * sizeof(KChar), otherRaw, other->count_ * sizeof(KChar));
    if (result == nullptr)
        return -1;

    return (reinterpret_cast<intptr_t>(result) - reinterpret_cast<intptr_t>(CharArrayAddressOfElementAt(thiz, 0))) /
        sizeof(KChar);
}

KInt Kotlin_String_lastIndexOfString(KString thiz, KString other, KInt fromIndex) {
    KInt count = thiz->count_;
    KInt otherCount = other->count_;

    if (fromIndex < 0 || otherCount > count) {
        return -1;
    }
    if (otherCount == 0) {
        return fromIndex < count ? fromIndex : count;
    }

    KInt start = fromIndex;
    if (fromIndex > count - otherCount)
        start = count - otherCount;
    KChar firstChar = *CharArrayAddressOfElementAt(other, 0);
    while (true) {
        KInt candidate = Kotlin_String_lastIndexOfChar(thiz, firstChar, start);
        if (candidate == -1)
            return -1;
        KInt offsetThiz = candidate;
        KInt offsetOther = 0;
        while (++offsetOther < otherCount &&
               *CharArrayAddressOfElementAt(thiz, ++offsetThiz) == *CharArrayAddressOfElementAt(other, offsetOther)) {
        }
        if (offsetOther == otherCount) {
            return candidate;
        }
        start = candidate - 1;
    }
}

KInt Kotlin_String_hashCode(KString thiz) {
    // TODO: consider caching strings hashes.
    // TODO: maybe use some simpler hashing algorithm?
    // Note that we don't use Java's string hash.
    return CityHash64(CharArrayAddressOfElementAt(thiz, 0), thiz->count_ * sizeof(KChar));
}

const KChar* Kotlin_String_utf16pointer(KString message) {
    RuntimeAssert(message->type_info() == theStringTypeInfo, "Must use a string");
    const KChar* utf16 = CharArrayAddressOfElementAt(message, 0);
    return utf16;
}

KInt Kotlin_String_utf16length(KString message) {
    RuntimeAssert(message->type_info() == theStringTypeInfo, "Must use a string");
    return message->count_ * sizeof(KChar);
}


} // extern "C"
