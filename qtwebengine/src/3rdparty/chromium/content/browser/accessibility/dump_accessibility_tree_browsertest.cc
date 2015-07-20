// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

// TODO(aboxhall): Create expectations on Android for these
#if defined(OS_ANDROID)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

namespace content {

namespace {

const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kMarkEndOfFile[] = "<-- End-of-file -->";
const char kSignalDiff[] = "*";

}  // namespace

typedef AccessibilityTreeFormatter::Filter Filter;

// This test takes a snapshot of the platform BrowserAccessibility tree and
// tests it against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from chrome/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page and serialize the platform specific tree into a human
//    readable string.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityTreeTest : public ContentBrowserTest {
 public:
  // Utility helper that does a comment aware equality check.
  // Returns array of lines from expected file which are different.
  std::vector<int> DiffLines(const std::vector<std::string>& expected_lines,
                             const std::vector<std::string>& actual_lines) {
    int actual_lines_count = actual_lines.size();
    int expected_lines_count = expected_lines.size();
    std::vector<int> diff_lines;
    int i = 0, j = 0;
    while (i < actual_lines_count && j < expected_lines_count) {
      if (expected_lines[j].size() == 0 ||
          expected_lines[j][0] == kCommentToken) {
        // Skip comment lines and blank lines in expected output.
        ++j;
        continue;
      }

      if (actual_lines[i] != expected_lines[j])
        diff_lines.push_back(j);
      ++i;
      ++j;
    }

    // Actual file has been fully checked.
    return diff_lines;
  }

  void AddDefaultFilters(std::vector<Filter>* filters) {
    filters->push_back(Filter(base::ASCIIToUTF16("FOCUSABLE"), Filter::ALLOW));
    filters->push_back(Filter(base::ASCIIToUTF16("READONLY"), Filter::ALLOW));
    filters->push_back(Filter(base::ASCIIToUTF16("name*"), Filter::ALLOW));
    filters->push_back(Filter(base::ASCIIToUTF16("*=''"), Filter::DENY));
  }

  // Parse the test html file and parse special directives, usually
  // beginning with an '@' and inside an HTML comment, that control how the
  // test is run and how the results are interpreted.
  //
  // When the accessibility tree is dumped as text, each attribute is
  // run through filters before being appended to the string. An "allow"
  // filter specifies attribute strings that should be dumped, and a "deny"
  // filter specifies strings that should be suppressed. As an example,
  // @MAC-ALLOW:AXSubrole=* means that the AXSubrole attribute should be
  // printed, while @MAC-ALLOW:AXSubrole=AXList* means that any subrole
  // beginning with the text "AXList" should be printed.
  //
  // The @WAIT-FOR:text directive allows the test to specify that the document
  // may dynamically change after initial load, and the test is to wait
  // until the given string (e.g., "text") appears in the resulting dump.
  // A test can make some changes to the document, then append a magic string
  // indicating that the test is done, and this framework will wait for that
  // string to appear before comparing the results.
  void ParseHtmlForExtraDirectives(const std::string& test_html,
                                   std::vector<Filter>* filters,
                                   std::string* wait_for) {
    std::vector<std::string> lines;
    base::SplitString(test_html, '\n', &lines);
    for (std::vector<std::string>::const_iterator iter = lines.begin();
         iter != lines.end();
         ++iter) {
      const std::string& line = *iter;
      const std::string& allow_empty_str =
          AccessibilityTreeFormatter::GetAllowEmptyString();
      const std::string& allow_str =
          AccessibilityTreeFormatter::GetAllowString();
      const std::string& deny_str =
          AccessibilityTreeFormatter::GetDenyString();
      const std::string& wait_str = "@WAIT-FOR:";
      if (StartsWithASCII(line, allow_empty_str, true)) {
        filters->push_back(
          Filter(base::UTF8ToUTF16(line.substr(allow_empty_str.size())),
                 Filter::ALLOW_EMPTY));
      } else if (StartsWithASCII(line, allow_str, true)) {
        filters->push_back(Filter(base::UTF8ToUTF16(
                                      line.substr(allow_str.size())),
                                  Filter::ALLOW));
      } else if (StartsWithASCII(line, deny_str, true)) {
        filters->push_back(Filter(base::UTF8ToUTF16(
                                      line.substr(deny_str.size())),
                                  Filter::DENY));
      } else if (StartsWithASCII(line, wait_str, true)) {
        *wait_for = line.substr(wait_str.size());
      }
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // Enable <dialog>, which is used in some tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void RunTest(const base::FilePath::CharType* file_path);
};

void DumpAccessibilityTreeTest::RunTest(
    const base::FilePath::CharType* file_path) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Setup test paths.
  base::FilePath dir_test_data;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &dir_test_data));
  base::FilePath test_path(
      dir_test_data.Append(FILE_PATH_LITERAL("accessibility")));
  ASSERT_TRUE(base::PathExists(test_path))
      << test_path.LossyDisplayName();

  base::FilePath html_file = test_path.Append(base::FilePath(file_path));
  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  printf("Testing: %s\n", html_file.MaybeAsASCII().c_str());

  std::string html_contents;
  base::ReadFileToString(html_file, &html_contents);

  // Read the expected file.
  std::string expected_contents_raw;
  base::FilePath expected_file =
    base::FilePath(html_file.RemoveExtension().value() +
                   AccessibilityTreeFormatter::GetExpectedFileSuffix());
  base::ReadFileToString(expected_file, &expected_contents_raw);

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    printf("Skipping this test on this platform.\n");
    return;
  }

  // Parse filters and other directives in the test file.
  std::vector<Filter> filters;
  std::string wait_for;
  AddDefaultFilters(&filters);
  ParseHtmlForExtraDirectives(html_contents, &filters, &wait_for);

  // Load the page.
  base::string16 html_contents16;
  html_contents16 = base::UTF8ToUTF16(html_contents);
  GURL url = GetTestUrl("accessibility",
                        html_file.BaseName().MaybeAsASCII().c_str());

  // If there's a @WAIT-FOR directive, set up an accessibility notification
  // waiter that returns on any event; we'll stop when we get the text we're
  // waiting for, or time out. Otherwise just wait specifically for
  // the "load complete" event.
  scoped_ptr<AccessibilityNotificationWaiter> waiter;
  if (!wait_for.empty()) {
    waiter.reset(new AccessibilityNotificationWaiter(
        shell(), AccessibilityModeComplete, ui::AX_EVENT_NONE));
  } else {
    waiter.reset(new AccessibilityNotificationWaiter(
        shell(), AccessibilityModeComplete, ui::AX_EVENT_LOAD_COMPLETE));
  }

  // Load the test html.
  NavigateToURL(shell(), url);

  // Wait for notifications. If there's a @WAIT-FOR directive, break when
  // the text we're waiting for appears in the dump, otherwise break after
  // the first notification, which will be a load complete.
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  std::string actual_contents;
  do {
    waiter->WaitForNotification();
    base::string16 actual_contents_utf16;
    AccessibilityTreeFormatter formatter(
        web_contents->GetRootBrowserAccessibilityManager()->GetRoot());
    formatter.SetFilters(filters);
    formatter.FormatAccessibilityTree(&actual_contents_utf16);
    actual_contents = base::UTF16ToUTF8(actual_contents_utf16);
  } while (!wait_for.empty() &&
           actual_contents.find(wait_for) == std::string::npos);

  // Perform a diff (or write the initial baseline).
  std::vector<std::string> actual_lines, expected_lines;
  Tokenize(actual_contents, "\n", &actual_lines);
  Tokenize(expected_contents, "\n", &expected_lines);
  // Marking the end of the file with a line of text ensures that
  // file length differences are found.
  expected_lines.push_back(kMarkEndOfFile);
  actual_lines.push_back(kMarkEndOfFile);

  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  EXPECT_FALSE(is_different);
  if (is_different) {
    // Mark the expected lines which did not match actual output with a *.
    printf("* Line Expected\n");
    printf("- ---- --------\n");
    for (int line = 0, diff_index = 0;
         line < static_cast<int>(expected_lines.size());
         ++line) {
      bool is_diff = false;
      if (diff_index < static_cast<int>(diff_lines.size()) &&
          diff_lines[diff_index] == line) {
        is_diff = true;
        ++diff_index;
      }
      printf("%1s %4d %s\n", is_diff? kSignalDiff : "", line + 1,
             expected_lines[line].c_str());
    }
    printf("\nActual\n");
    printf("------\n");
    printf("%s\n", actual_contents.c_str());
  }

  if (!base::PathExists(expected_file)) {
    base::FilePath actual_file =
        base::FilePath(html_file.RemoveExtension().value() +
                       AccessibilityTreeFormatter::GetActualFileSuffix());

    EXPECT_TRUE(base::WriteFile(
        actual_file, actual_contents.c_str(), actual_contents.size()));

    ADD_FAILURE() << "No expectation found. Create it by doing:\n"
                  << "mv " << actual_file.LossyDisplayName() << " "
                  << expected_file.LossyDisplayName();
  }
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityA) {
  RunTest(FILE_PATH_LITERAL("a.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAbbr) {
  RunTest(FILE_PATH_LITERAL("abbr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAddress) {
  RunTest(FILE_PATH_LITERAL("address.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityArea) {
  RunTest(FILE_PATH_LITERAL("area.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAName) {
  RunTest(FILE_PATH_LITERAL("a-name.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityANameCalc) {
  RunTest(FILE_PATH_LITERAL("a-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityANoText) {
  RunTest(FILE_PATH_LITERAL("a-no-text.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAOnclick) {
  RunTest(FILE_PATH_LITERAL("a-onclick.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaActivedescendant) {
  RunTest(FILE_PATH_LITERAL("aria-activedescendant.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaAlert) {
  RunTest(FILE_PATH_LITERAL("aria-alert.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaApplication) {
  RunTest(FILE_PATH_LITERAL("aria-application.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaArticle) {
  RunTest(FILE_PATH_LITERAL("aria-article.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaAtomic) {
  RunTest(FILE_PATH_LITERAL("aria-atomic.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaAutocomplete) {
  RunTest(FILE_PATH_LITERAL("aria-autocomplete.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaBanner) {
  RunTest(FILE_PATH_LITERAL("aria-banner.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaBusy) {
  RunTest(FILE_PATH_LITERAL("aria-busy.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaButton) {
  RunTest(FILE_PATH_LITERAL("aria-button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaCheckBox) {
  RunTest(FILE_PATH_LITERAL("aria-checkbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaChecked) {
  RunTest(FILE_PATH_LITERAL("aria-checked.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaColumnHeader) {
  RunTest(FILE_PATH_LITERAL("aria-columnheader.html"));
}

// crbug.com/98976 will cause new elements to be added to the Blink a11y tree
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaCombobox) {
  RunTest(FILE_PATH_LITERAL("aria-combobox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaComplementary) {
  RunTest(FILE_PATH_LITERAL("aria-complementary.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaContentInfo) {
  RunTest(FILE_PATH_LITERAL("aria-contentinfo.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDefinition) {
  RunTest(FILE_PATH_LITERAL("aria-definition.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDialog) {
  RunTest(FILE_PATH_LITERAL("aria-dialog.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaExpanded) {
  RunTest(FILE_PATH_LITERAL("aria-expanded.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaHasPopup) {
  RunTest(FILE_PATH_LITERAL("aria-haspopup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaHeading) {
  RunTest(FILE_PATH_LITERAL("aria-heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaHidden) {
  RunTest(FILE_PATH_LITERAL("aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       MAYBE(AccessibilityAriaFlowto)) {
  RunTest(FILE_PATH_LITERAL("aria-flowto.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaImg) {
  RunTest(FILE_PATH_LITERAL("aria-img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaInvalid) {
  RunTest(FILE_PATH_LITERAL("aria-invalid.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByHeading) {
  RunTest(FILE_PATH_LITERAL("aria-labelledby-heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLevel) {
  RunTest(FILE_PATH_LITERAL("aria-level.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaList) {
  RunTest(FILE_PATH_LITERAL("aria-list.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaListBox) {
  RunTest(FILE_PATH_LITERAL("aria-listbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxActiveDescendant) {
  RunTest(FILE_PATH_LITERAL("aria-listbox-activedescendant.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxAriaSelected) {
  RunTest(FILE_PATH_LITERAL("aria-listbox-aria-selected.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxChildFocus) {
  RunTest(FILE_PATH_LITERAL("aria-listbox-childfocus.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLive) {
  RunTest(FILE_PATH_LITERAL("aria-live.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaLiveWithContent) {
  RunTest(FILE_PATH_LITERAL("aria-live-with-content.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLog) {
  RunTest(FILE_PATH_LITERAL("aria-log.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMarquee) {
  RunTest(FILE_PATH_LITERAL("aria-marquee.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMenu) {
  RunTest(FILE_PATH_LITERAL("aria-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMenuBar) {
  RunTest(FILE_PATH_LITERAL("aria-menubar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuItemCheckBox) {
  RunTest(FILE_PATH_LITERAL("aria-menuitemcheckbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuItemRadio) {
  RunTest(FILE_PATH_LITERAL("aria-menuitemradio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaNavigation) {
  RunTest(FILE_PATH_LITERAL("aria-navigation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaOrientation) {
  RunTest(FILE_PATH_LITERAL("aria-orientation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMath) {
  RunTest(FILE_PATH_LITERAL("aria-math.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaNone) {
  RunTest(FILE_PATH_LITERAL("aria-none.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaPresentation) {
  RunTest(FILE_PATH_LITERAL("aria-presentation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaPressed) {
  RunTest(FILE_PATH_LITERAL("aria-pressed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaProgressbar) {
  RunTest(FILE_PATH_LITERAL("aria-progressbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaRadiogroup) {
  RunTest(FILE_PATH_LITERAL("aria-radiogroup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRelevant) {
  RunTest(FILE_PATH_LITERAL("aria-relevant.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRequired) {
  RunTest(FILE_PATH_LITERAL("aria-required.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRow) {
  RunTest(FILE_PATH_LITERAL("aria-row.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRowGroup) {
  RunTest(FILE_PATH_LITERAL("aria-rowgroup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaReadonly) {
  RunTest(FILE_PATH_LITERAL("aria-readonly.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRegion) {
  RunTest(FILE_PATH_LITERAL("aria-region.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSearch) {
  RunTest(FILE_PATH_LITERAL("aria-search.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSeparator) {
  RunTest(FILE_PATH_LITERAL("aria-separator.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSort) {
  RunTest(FILE_PATH_LITERAL("aria-sort.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaSpinButton) {
  RunTest(FILE_PATH_LITERAL("aria-spinbutton.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTextbox) {
  RunTest(FILE_PATH_LITERAL("aria-textbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTimer) {
  RunTest(FILE_PATH_LITERAL("aria-timer.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaToggleButton) {
  RunTest(FILE_PATH_LITERAL("aria-togglebutton.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaToolbar) {
  RunTest(FILE_PATH_LITERAL("aria-toolbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTree) {
  RunTest(FILE_PATH_LITERAL("aria-tree.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaValueMin) {
  RunTest(FILE_PATH_LITERAL("aria-valuemin.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaValueMax) {
  RunTest(FILE_PATH_LITERAL("aria-valuemax.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaValueNow) {
  RunTest(FILE_PATH_LITERAL("aria-valuenow.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityArticle) {
  RunTest(FILE_PATH_LITERAL("article.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAside) {
  RunTest(FILE_PATH_LITERAL("aside.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAudio) {
  RunTest(FILE_PATH_LITERAL("audio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAWithImg) {
  RunTest(FILE_PATH_LITERAL("a-with-img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBdo) {
  RunTest(FILE_PATH_LITERAL("bdo.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBlockquote) {
  RunTest(FILE_PATH_LITERAL("blockquote.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBody) {
  RunTest(FILE_PATH_LITERAL("body.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBR) {
  RunTest(FILE_PATH_LITERAL("br.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityButton) {
  RunTest(FILE_PATH_LITERAL("button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityButtonNameCalc) {
  RunTest(FILE_PATH_LITERAL("button-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCanvas) {
  RunTest(FILE_PATH_LITERAL("canvas.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCaption) {
  RunTest(FILE_PATH_LITERAL("caption.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCheckboxNameCalc) {
  RunTest(FILE_PATH_LITERAL("checkbox-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCite) {
  RunTest(FILE_PATH_LITERAL("cite.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCol) {
  RunTest(FILE_PATH_LITERAL("col.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityColgroup) {
  RunTest(FILE_PATH_LITERAL("colgroup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDel) {
  RunTest(FILE_PATH_LITERAL("del.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDetails) {
  RunTest(FILE_PATH_LITERAL("details.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDfn) {
  RunTest(FILE_PATH_LITERAL("dfn.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDialog) {
  RunTest(FILE_PATH_LITERAL("dialog.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDiv) {
  RunTest(FILE_PATH_LITERAL("div.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDl) {
  RunTest(FILE_PATH_LITERAL("dl.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDt) {
  RunTest(FILE_PATH_LITERAL("dt.html"));
}

//Disabled because of https://codereview.chromium.org/696953002 temporarily.
//After blink code is merged, it will be enabled.
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityContenteditableDescendants) {
  RunTest(FILE_PATH_LITERAL("contenteditable-descendants.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityEm) {
  RunTest(FILE_PATH_LITERAL("em.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFieldset) {
  RunTest(FILE_PATH_LITERAL("fieldset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFigcaption) {
  RunTest(FILE_PATH_LITERAL("figcaption.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFigure) {
  RunTest(FILE_PATH_LITERAL("figure.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFooter) {
  RunTest(FILE_PATH_LITERAL("footer.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityForm) {
  RunTest(FILE_PATH_LITERAL("form.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFrameset) {
  RunTest(FILE_PATH_LITERAL("frameset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHead) {
  RunTest(FILE_PATH_LITERAL("head.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHeader) {
  RunTest(FILE_PATH_LITERAL("header.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHeading) {
  RunTest(FILE_PATH_LITERAL("heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHR) {
  RunTest(FILE_PATH_LITERAL("hr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityI) {
  RunTest(FILE_PATH_LITERAL("i.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityIframe) {
  RunTest(FILE_PATH_LITERAL("iframe.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeCoordinates) {
  RunTest(FILE_PATH_LITERAL("iframe-coordinates.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityImg) {
  RunTest(FILE_PATH_LITERAL("img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputButton) {
  RunTest(FILE_PATH_LITERAL("input-button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputButtonInMenu) {
  RunTest(FILE_PATH_LITERAL("input-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputCheckBox) {
  RunTest(FILE_PATH_LITERAL("input-checkbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputCheckBoxInMenu) {
  RunTest(FILE_PATH_LITERAL("input-checkbox-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputColor) {
  RunTest(FILE_PATH_LITERAL("input-color.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
    DISABLED_AccessibilityInputDate) {
  RunTest(FILE_PATH_LITERAL("input-date.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputDateTime) {
  RunTest(FILE_PATH_LITERAL("input-datetime.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputDateTimeLocal) {
#if defined(OS_MACOSX)
  // Fails on OS X 10.9 <https://crbug.com/430622>.
  if (base::mac::IsOSMavericks())
    return;
#endif
  RunTest(FILE_PATH_LITERAL("input-datetime-local.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputImageButtonInMenu) {
  RunTest(FILE_PATH_LITERAL("input-image-button-in-menu.html"));
}

// crbug.com/423675 - AX tree is different for Win7 and Win8.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityInputMonth) {
  RunTest(FILE_PATH_LITERAL("input-month.html"));
}
#else
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputMonth) {
  RunTest(FILE_PATH_LITERAL("input-month.html"));
}
#endif

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputRadio) {
  RunTest(FILE_PATH_LITERAL("input-radio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityInputRange) {
  RunTest(FILE_PATH_LITERAL("input-range.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputReset) {
  RunTest(FILE_PATH_LITERAL("input-reset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputSearch) {
  RunTest(FILE_PATH_LITERAL("input-search.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputSubmit) {
  RunTest(FILE_PATH_LITERAL("input-submit.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputTel) {
  RunTest(FILE_PATH_LITERAL("input-tel.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputText) {
  RunTest(FILE_PATH_LITERAL("input-text.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTextNameCalc) {
  RunTest(FILE_PATH_LITERAL("input-text-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputTextValue) {
  RunTest(FILE_PATH_LITERAL("input-text-value.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
    DISABLED_AccessibilityInputTime) {
  RunTest(FILE_PATH_LITERAL("input-time.html"));
}

// crbug.com/98976 will cause new elements to be added to the Blink a11y tree
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTypes) {
  RunTest(FILE_PATH_LITERAL("input-types.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputUrl) {
  RunTest(FILE_PATH_LITERAL("input-url.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputWeek) {
  RunTest(FILE_PATH_LITERAL("input-week.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityIns) {
  RunTest(FILE_PATH_LITERAL("ins.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLabel) {
  RunTest(FILE_PATH_LITERAL("label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLandmark) {
  RunTest(FILE_PATH_LITERAL("landmark.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLegend) {
  RunTest(FILE_PATH_LITERAL("legend.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLink) {
  RunTest(FILE_PATH_LITERAL("link.html"));
}

//Disabled because of https://codereview.chromium.org/696953002 temporarily.
//After blink code is merged, it will be enabled.
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, DISABLED_AccessibilityList) {
  RunTest(FILE_PATH_LITERAL("list.html"));
}

//Disabled because of https://codereview.chromium.org/696953002 temporarily.
//After blink code is merged, it will be enabled.
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityListMarkers) {
  RunTest(FILE_PATH_LITERAL("list-markers.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMain) {
  RunTest(FILE_PATH_LITERAL("main.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMark) {
  RunTest(FILE_PATH_LITERAL("mark.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMath) {
  RunTest(FILE_PATH_LITERAL("math.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityMenutypecontext) {
  RunTest(FILE_PATH_LITERAL("menu-type-context.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMeta) {
  RunTest(FILE_PATH_LITERAL("meta.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMeter) {
  RunTest(FILE_PATH_LITERAL("meter.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogClosed) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-closed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogOpened) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-opened.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeClosed) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-closed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeOpened) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-opened.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogStack) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-stack.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityNavigation) {
  RunTest(FILE_PATH_LITERAL("navigation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityNoscript) {
  RunTest(FILE_PATH_LITERAL("noscript.html"));
}

//Disabled because of https://codereview.chromium.org/696953002 temporarily.
//After blink code is merged, it will be enabled.
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, DISABLED_AccessibilityOl) {
  RunTest(FILE_PATH_LITERAL("ol.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityObject) {
  RunTest(FILE_PATH_LITERAL("object.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityOptionindatalist) {
  RunTest(FILE_PATH_LITERAL("option-in-datalist.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityOutput) {
  RunTest(FILE_PATH_LITERAL("output.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityP) {
  RunTest(FILE_PATH_LITERAL("p.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityParam) {
  RunTest(FILE_PATH_LITERAL("param.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityPre) {
  RunTest(FILE_PATH_LITERAL("pre.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityProgress) {
  RunTest(FILE_PATH_LITERAL("progress.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityQ) {
  RunTest(FILE_PATH_LITERAL("q.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityS) {
  RunTest(FILE_PATH_LITERAL("s.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySamp) {
  RunTest(FILE_PATH_LITERAL("samp.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySection) {
  RunTest(FILE_PATH_LITERAL("section.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySelect) {
  RunTest(FILE_PATH_LITERAL("select.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySource) {
  RunTest(FILE_PATH_LITERAL("source.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySpan) {
  RunTest(FILE_PATH_LITERAL("span.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySub) {
  RunTest(FILE_PATH_LITERAL("sub.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySummary) {
  RunTest(FILE_PATH_LITERAL("summary.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySvg) {
  RunTest(FILE_PATH_LITERAL("svg.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTab) {
  RunTest(FILE_PATH_LITERAL("tab.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTableSimple) {
  RunTest(FILE_PATH_LITERAL("table-simple.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                    AccessibilityTableThRowHeader) {
  RunTest(FILE_PATH_LITERAL("table-th-rowheader.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                    AccessibilityTableTbodyTfoot) {
  RunTest(FILE_PATH_LITERAL("table-thead-tbody-tfoot.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTableSpans) {
  RunTest(FILE_PATH_LITERAL("table-spans.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTextArea) {
  RunTest(FILE_PATH_LITERAL("textarea.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTime) {
  RunTest(FILE_PATH_LITERAL("time.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTitle) {
  RunTest(FILE_PATH_LITERAL("title.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTransition) {
  RunTest(FILE_PATH_LITERAL("transition.html"));
}

//Disabled because of https://codereview.chromium.org/696953002 temporarily.
//After blink code is merged, it will be enabled.
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, DISABLED_AccessibilityUl) {
  RunTest(FILE_PATH_LITERAL("ul.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityVar) {
  RunTest(FILE_PATH_LITERAL("var.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityWbr) {
  RunTest(FILE_PATH_LITERAL("wbr.html"));
}

}  // namespace content
