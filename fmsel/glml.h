/* FMSel is free software; you can redistribute it and/or modify
 * it under the terms of the FLTK License.
 *
 * FMSel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * FLTK License for more details.
 *
 * You should have received a copy of the FLTK License along with
 * FMSel.
 */

#ifndef _GLML_H_
#define _GLML_H_


#include <string>

bool IsGlmlFile(const std::string &filename);

// ad-hoc conversion of a garrettloader proprietary 'glml' file into simple html
std::string GlmlToHtml(const std::string &filename, const std::string &dirname);

// extract the fm title from a glml file-in-a-string
std::string GlmlGetTitle(const std::string &file);

#endif // _GLML_H_
