/* SPDX-License-Identifier: Apache-2.0
 * Analytic interior samples shared by host and opt-in board probes. */
#ifndef LV_AIC_SVG_DOCUMENT_CASES_H
#define LV_AIC_SVG_DOCUMENT_CASES_H
static const struct {
    const char *name,*body;
    struct {int x,y;uint32_t rgb;} sample[3];
} svg_document_cases[]={
    {"group-opacity","<g opacity='0.5'><rect x='4' y='4' width='24' height='24' fill='#ff0000'/><rect x='16' y='4' width='24' height='24' fill='#0000ff'/></g>",
     {{8,8,0x800000},{20,8,0x000080},{36,8,0x000080}}},
    {"nested-opacity","<g opacity='0.5'><rect x='4' y='4' width='24' height='24' fill='#ff0000'/><g opacity='0.5'><rect x='16' y='4' width='24' height='24' fill='#0000ff'/></g></g><rect x='48' y='4' width='8' height='8' fill='#00ff00'/>",
     {{8,8,0x800000},{20,8,0x400040},{52,8,0x00ff00}}},
    {"element-opacity","<rect x='4' y='4' width='24' height='24' fill='#ff0000' stroke='#0000ff' stroke-width='8' opacity='0.5'/>",
     {{12,12,0x800000},{6,12,0x000080},{40,40,0}}},
    {"use-opacity","<defs><g id='tile'><rect width='24' height='24' fill='#ff0000'/><rect x='12' width='24' height='24' fill='#0000ff'/></g></defs><use x='4' y='4' xlink:href='#tile' opacity='0.5'/>",
     {{8,8,0x800000},{20,8,0x000080},{36,8,0x000080}}},
    {"opacity-is-not-inherited","<g opacity='0.5'><g><rect x='4' y='4' width='24' height='24' fill='#ff0000'/></g></g>",
     {{8,8,0x800000},{20,8,0x800000},{40,40,0}}},
    {"opacity-inherit","<g opacity='0.5'><g opacity='inherit'><rect x='4' y='4' width='24' height='24' fill='#ff0000'/></g><g><rect x='36' y='4' width='20' height='24' fill='#0000ff' opacity='inherit'/></g></g>",
     {{8,8,0x400000},{40,8,0x000080},{60,60,0}}},
    {"opacity-endpoints","<rect width='16' height='16' fill='#ff0000' opacity='0'/><rect x='20' width='16' height='16' fill='#00ff00' opacity='1'/><g opacity='0'><rect x='40' width='16' height='16' fill='#0000ff' opacity='1'/></g>",
     {{8,8,0},{24,8,0x00ff00},{48,8,0}}},
    {"opacity-order","<rect width='32' height='32' fill='#ff0000'/><g opacity='0.5'><rect x='8' width='32' height='32' fill='#0000ff'/></g><rect x='24' width='24' height='32' fill='#00ff00'/>",
     {{4,8,0xff0000},{12,8,0x7f0080},{28,8,0x00ff00}}},
    {"opacity-transform","<g transform='translate(8 8) scale(2)' opacity='0.5'><rect width='8' height='8' fill='#ff0000'/><g transform='translate(12 0)' opacity='0.5'><rect width='8' height='8' fill='#0000ff'/></g></g>",
     {{12,12,0x800000},{36,12,0x000040},{4,4,0}}},
    {"opacity-use-transform","<defs><g id='tile' transform='scale(2)' opacity='0.5'><rect width='8' height='8' fill='#ff0000'/></g></defs><use x='10' y='10' xlink:href='#tile' opacity='0.5'/>",
     {{12,12,0x400000},{23,23,0x400000},{31,31,0}}},
    {"opacity-fill","<g opacity='0.5'><rect x='4' y='4' width='24' height='24' fill='#ff0000' fill-opacity='0.5'/><rect x='16' y='4' width='24' height='24' fill='#0000ff' fill-opacity='0.5'/></g>",
     {{8,8,0x400000},{20,8,0x200040},{36,8,0x000040}}},
    {"opacity-root","<svg width='64' height='64' viewBox='0 0 32 32' opacity='0.5' xmlns='http://www.w3.org/2000/svg'><rect x='2' y='2' width='12' height='12' fill='#ff0000'/><rect x='8' y='2' width='12' height='12' fill='#0000ff'/></svg>",
     {{8,8,0x800000},{20,8,0x000080},{36,8,0x000080}}},
    {"missing-image","<image width='8' height='8' transform='translate(30 0)' xlink:href='missing.png'/><rect x='4' y='4' width='8' height='8' fill='#ff0000'/>",
     {{7,7,0xff0000},{37,7,0},{60,60,0}}},
    {"use-transform","<defs><g id='tile' transform='scale(2)'><rect width='8' height='8' fill='#ff0000'/></g></defs><use x='10' y='10' xlink:href='#tile'/>",
     {{12,12,0xff0000},{23,23,0xff0000},{31,31,0}}},
    {"use-style","<defs><rect id='tile' width='8' height='8' fill='#ff0000'/></defs><use x='10' y='10' fill='#00ff00' xlink:href='#tile'/>",
     {{13,13,0xff0000},{4,4,0},{30,30,0}}},
    {"inherit-style","<defs><g id='tile'><rect width='8' height='8'/></g></defs><use x='10' y='10' fill='#00ff00' xlink:href='#tile'/>",
     {{13,13,0x00ff00},{4,4,0},{30,30,0}}},
    {"nested-use","<defs><g id='tile' transform='scale(2)'><rect width='4' height='4' fill='#ff0000'/></g><use id='middle' x='3' y='2' xlink:href='#tile'/></defs><use x='5' y='7' xlink:href='#middle'/>",
     {{10,11,0xff0000},{20,20,0},{4,4,0}}},
    {"rotated-use","<defs><rect id='tile' width='8' height='4' fill='#ff0000' transform='scale(2)'/></defs><use x='4' y='3' transform='translate(40 10) rotate(90)' xlink:href='#tile'/>",
     {{33,20,0xff0000},{40,20,0},{10,10,0}}},
    {"missing-use","<defs><rect id='tile' width='8' height='8'/></defs><use x='10'/><rect x='4' y='4' width='8' height='8' fill='#0000ff'/>",
     {{7,7,0x0000ff},{16,4,0},{60,60,0}}},
    {"missing-reference","<use xlink:href='#absent'/><rect x='4' y='4' width='8' height='8' fill='#0000ff'/>",
     {{7,7,0x0000ff},{16,4,0},{60,60,0}}},
    {"cycle","<defs><g id='cycle'><use xlink:href='#cycle'/></g></defs><use xlink:href='#cycle'/><rect x='4' y='4' width='8' height='8' fill='#0000ff'/>",
     {{7,7,0x0000ff},{16,4,0},{60,60,0}}},
    {"mutual-cycle","<defs><use id='a' xlink:href='#b'/><use id='b' xlink:href='#a'/></defs><use xlink:href='#a'/><rect x='4' y='4' width='8' height='8' fill='#0000ff'/>",
     {{7,7,0x0000ff},{16,4,0},{60,60,0}}},
};
#endif
