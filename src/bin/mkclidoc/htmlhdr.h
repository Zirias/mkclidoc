#define HTML_DEFAULT_STYLE \
"@media (prefers-color-scheme: dark) {\n" \
"  :root {\n" \
"    --bgcol: #2e2b28;\n" \
"    --fgcol: #c8c6c5;\n" \
"    --strongcol: #4576e6;\n" \
"    --emcol: #41d9c2;\n" \
"  }\n" \
"}\n" \
"@media (prefers-color-scheme: light) {\n" \
"  :root {\n" \
"    --bgcol: #c8c6c5;\n" \
"    --fgcol: black;\n" \
"    --strongcol: #193c83;\n" \
"    --emcol: #346a6f;\n" \
"  }\n" \
"}\n" \
"* {\n" \
"  font-family: monospace;\n" \
"  font-size: medium;\n" \
"  margin: 0;\n" \
"  padding: 0;\n" \
"  color: var(--fgcol);\n" \
"}\n" \
"body {\n" \
"  position: absolute;\n" \
"  padding-left: 3.5ch;\n" \
"  padding-right: .5ch;\n" \
"  padding-top: .5em;\n" \
"  padding-bottom: 2em;\n" \
"  background-color: var(--bgcol);\n" \
"}\n" \
"h1, h2 {\n" \
"  margin-left: -3ch;\n" \
"}\n" \
"h3 {\n" \
"  margin-left: -1ch;\n" \
"}\n" \
"h1 {\n" \
"  font-weight: normal;\n" \
"}\n" \
"h2, h3 {\n" \
"  clear: left;\n" \
"  padding-top: 1em;\n" \
"  font-weight: bold;\n" \
"  color: var(--strongcol);\n" \
"}\n" \
"h1:before {\n" \
"  position: absolute;\n" \
"  right: 1ch;\n" \
"  content: attr(data-man-sectionname);\n" \
"}\n" \
"p {\n" \
"  margin-top: 1em;\n" \
"}\n" \
"h2 + p, h3 + p {\n" \
"  margin-top: 0;\n" \
"  gap: 0;\n" \
"}\n" \
"table {\n" \
"  border-spacing: 0;\n" \
"}\n" \
"td {\n" \
"  vertical-align: top;\n" \
"  padding-right: 1ch;\n" \
"}\n" \
"td:last-child {\n" \
"  padding-right: 0;\n" \
"}\n" \
"p + table {\n" \
"  margin-top: 1em;\n" \
"}\n" \
"dl {\n" \
"  clear: left;\n" \
"  padding-top: 1em;\n" \
"}\n" \
"h2 + dl, h3 + dl {\n" \
"  padding-top: 0;\n" \
"}\n" \
"dd {\n" \
"  width: 100%;\n" \
"}\n" \
"dl.name, dl.synopsis {\n" \
"  display: grid;\n" \
"  grid-template-columns: auto 1fr;\n" \
"  gap: 0;\n" \
"}\n" \
"dl.name dt, dl.synopsis dt, dl.meta dt {\n" \
"  grid-column: 1;\n" \
"  margin-right: 1ch;\n" \
"}\n" \
"dl.synopsis dt {\n" \
"  font-weight: bold;\n" \
"  color: var(--strongcol);\n" \
"}\n" \
"dl.name dd, dl.synopsis dd, dl.meta dd {\n" \
"  grid-column: 2;\n" \
"}\n" \
"dl.description {\n" \
"  margin-left: 3ch;\n" \
"}\n" \
"dl.description > dt {\n" \
"  margin-top: 1em;\n" \
"  clear: left;\n" \
"  float: left;\n" \
"  margin-left: -3ch;\n" \
"  margin-bottom: -1em;\n" \
"}\n" \
"dl.description > dd {\n" \
"  margin-top: 1em;\n" \
"  float: left;\n" \
"}\n" \
"dl.description > dt:first-child, dl.description > dt:first-child + dd,\n" \
"dl dl.description dt, dl dl.description dd {\n" \
"  margin-top: 0;\n" \
"  margin-bottom: 0;\n" \
"}\n" \
"dl.description dd:has(> table) {\n" \
"  clear: left;\n" \
"}\n" \
"dl dd p:first-child {\n" \
"  margin-top: 0;\n" \
"}\n" \
"dl.meta {\n" \
"  display: grid;\n" \
"  grid-template-columns: auto 1fr;\n" \
"  gap: 0;\n" \
"}\n" \
"dl.footer {\n" \
"  margin-left: -3ch;\n" \
"  display: grid;\n" \
"  grid-template-columns: 1fr auto;\n" \
"  gap: 0;\n" \
"}\n" \
"dl.footer > dt, dl.footer > dd:last-child {\n" \
"  display: none;\n" \
"}\n" \
"span.link, span.name, span.flag, a {\n" \
"  font-weight: bold;\n" \
"  white-space: nowrap;\n" \
"  color: var(--strongcol);\n" \
"}\n" \
"a {\n" \
"  text-decoration: none;\n" \
"  font-weight: bold;\n" \
"  color: var(--strongcol);\n" \
"}\n" \
"a:hover {\n" \
"  text-decoration: underline;\n" \
"}\n" \
"span.arg, span.file {\n" \
"  text-decoration: underline;\n" \
"  color: var(--emcol);\n" \
"}\n" \
"@media only screen and (min-width: 80ch) {\n" \
"  body {\n" \
"    width: 73ch;\n" \
"    padding-left: 6.5ch;\n" \
"    padding-top: 1em;\n" \
"  }\n" \
"  h1, h2 { margin-left: -5ch; }\n" \
"  h3 { margin-left: -2ch; }\n" \
"  h1:before {\n" \
"    position: absolute;\n" \
"    right: initial;\n" \
"    left: 40ch;\n" \
"    transform: translateX(-50%);\n" \
"    content: attr(data-man-sectionname);\n" \
"  }\n" \
"  h1:after {\n" \
"    position: absolute;\n" \
"    left: 80ch;\n" \
"    margin-left: 0;\n" \
"    transform: translateX(-100%);\n" \
"    content: attr(data-man-title) \"(\" attr(data-man-section) \")\";\n" \
"  }\n" \
"  td { padding-right: 2ch; }\n" \
"  dl.description { margin-left: 8ch; }\n" \
"  dl.description > dt {\n" \
"    margin-left: -8ch;\n" \
"    padding-right: 1ch;\n" \
"  }\n" \
"  dl.meta dt {\n" \
"    margin-right: 2ch;\n" \
"  }\n" \
"  dl.footer {\n" \
"    margin-left: -5ch;\n" \
"    grid-template-columns: auto 1fr auto;\n" \
"  }\n" \
"  dl.footer > dd:last-child {\n" \
"    display: block;\n" \
"  }\n" \
"  dl.footer > dd {\n" \
"    text-align: center;\n" \
"  }\n" \
"}\n"

#define HTML_STYLE_START "<style>\n/* <![CDATA[ */\n"
#define HTML_STYLE_END "/* ]]> */\n</style>\n"
#define HTML_STYLEURI_START "<link rel=\"stylesheet\" href=\""
#define HTML_STYLEURI_END "\">\n"

#define HTML_HEADER_FMT \
"<!DOCTYPE HTML>\n" \
"<html lang=\"en\">\n" \
"<head>\n" \
"%s%s%s" \
"<title>%s(1)</title>\n" \
"<meta name=\"viewport\" content=\"width=device-width\">\n" \
"</head>\n" \
"<body>\n" \
"<h1 data-man-title=\"%s\" data-man-section=\"1\" " \
"data-man-sectionname=\"General Commands Manual\">%s(1)</h1>\n"

#define HTML_HEADER_STYLE(style, title) \
    HTML_HEADER_FMT, HTML_STYLE_START, (style), HTML_STYLE_END, \
    (title), (title), (title)

#define HTML_HEADER_STYLEURI(uri, title) \
    HTML_HEADER_FMT, HTML_STYLEURI_START, (uri), HTML_STYLEURI_END, \
    (title), (title), (title)
