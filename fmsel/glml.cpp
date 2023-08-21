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

#if defined(SUPPORT_T3) || defined(SUPPORT_GLML)

#include "glml.h"
#include <fstream>
#include <algorithm>


using std::string;


// replace every occurrence of a string within the range [start_pos, end_pos)
static void GlobalReplace(string &str, const string &search, const string &repl, size_t start_pos = 0, size_t end_pos = string::npos)
{
	size_t pos = start_pos;
	while (true)
	{
		pos = str.find(search, pos);
		if (pos >= end_pos)
			break;
		str.replace(pos, search.size(), repl);
	}
}

bool IsGlmlFile(const string &filename)
{
	// get the extension, lower case
	string ext;
	size_t pos = filename.find_last_of("./\\");
	if (pos != string::npos && filename[pos] == '.')
		ext = filename.substr(pos + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	return ext == "glml";
}

string GlmlToHtml(const string &filename, const string &dirname)
{
	if (!IsGlmlFile(filename))
		return "";

	// read the file into a string for conversion
#ifdef _WIN32
	string fl = dirname + "\\" + filename;
#else
	string fl = dirname + "/" + filename;
#endif
	std::ifstream ifs(fl.c_str());
	string html((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	// first replace gl-tags which have a 1:1 html counterpart
	GlobalReplace(html, "[GLNL]",        "<br>");
	GlobalReplace(html, "[GLCENTER]",    "<center>");
	GlobalReplace(html, "[/GLCENTER]",   "</center>");
	GlobalReplace(html, "[GLTITLE]",     "<h1>");
	GlobalReplace(html, "[/GLTITLE]",    "</h1>");
	GlobalReplace(html, "[GLSUBTITLE]",  "<h2>");
	GlobalReplace(html, "[/GLSUBTITLE]", "</h2>");
	GlobalReplace(html, "[GLWARNINGS]",  "<b>");
	GlobalReplace(html, "[/GLWARNINGS]", "</b>");
	GlobalReplace(html, "[GLLINE]",      "<hr><br><br>");
	GlobalReplace(html, "[GLLANGUAGE]",  "<p>");
	GlobalReplace(html, "[/GLLANGUAGE]", "</p>");

	// just remove every remaining tag
	GlobalReplace(html, "[GLAUTHOR]",       "");
	GlobalReplace(html, "[/GLAUTHOR]",      "");
	GlobalReplace(html, "[GLFMINFO]",       "");
	GlobalReplace(html, "[/GLFMINFO]",      "");
	GlobalReplace(html, "[GLBRIEFING]",     "");
	GlobalReplace(html, "[/GLBRIEFING]",    "");
	GlobalReplace(html, "[GLFMSTRUCTURE]",  "");
	GlobalReplace(html, "[/GLFMSTRUCTURE]", "");
	GlobalReplace(html, "[GLBUILD]",        "");
	GlobalReplace(html, "[/GLBUILD]",       "");
	GlobalReplace(html, "[GLINFO]",         "");
	GlobalReplace(html, "[/GLINFO]",        "");
	GlobalReplace(html, "[GLLOADING]",      "");
	GlobalReplace(html, "[/GLLOADING]",     "");
	GlobalReplace(html, "[GLCOPYRIGHT]",    "");
	GlobalReplace(html, "[/GLCOPYRIGHT]",   "");

	return html;
}

string GlmlGetTitle(const string &file)
{
	// todo: define tags centrally
	static const string title_start_tag = "[GLTITLE]";
	static const string title_end_tag = "[/GLTITLE]";

	size_t title_start = file.find(title_start_tag);
	if (title_start == string::npos)
		return "";
	title_start += title_start_tag.size();

	size_t title_end = file.find(title_end_tag, title_start);
	if (title_end == string::npos)
		return "";

	return file.substr(title_start, title_end - title_start);
}

#endif // defined(SUPPORT_T3) || defined(SUPPORT_GLML)
