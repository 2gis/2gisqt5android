// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_WIN_H_
#define UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_WIN_H_

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include <UIAutomationCore.h>

#include <set>
#include <vector>

#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"

namespace ui {
enum TextBoundaryDirection;
enum TextBoundaryType;
}

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// NativeViewAccessibilityWin
//
// Class implementing the MSAA IAccessible COM interface for a generic View,
// providing accessibility to be used by screen readers and other assistive
// technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class __declspec(uuid("26f5641a-246d-457b-a96d-07f3fae6acf2"))
NativeViewAccessibilityWin
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchImpl<IAccessible2_2, &IID_IAccessible2_2,
                           &LIBID_IAccessible2Lib>,
    public IAccessibleText,
    public IServiceProvider,
    public IAccessibleEx,
    public IRawElementProviderSimple,
    public NativeViewAccessibility {
 public:
  BEGIN_COM_MAP(NativeViewAccessibilityWin)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible2_2)
    COM_INTERFACE_ENTRY(IAccessible)
    COM_INTERFACE_ENTRY(IAccessible2)
    COM_INTERFACE_ENTRY(IAccessible2_2)
    COM_INTERFACE_ENTRY(IAccessibleEx)
    COM_INTERFACE_ENTRY(IAccessibleText)
    COM_INTERFACE_ENTRY(IRawElementProviderSimple)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()

  virtual ~NativeViewAccessibilityWin();

  // NativeViewAccessibility.
  void NotifyAccessibilityEvent(ui::AXEvent event_type) override;
  gfx::NativeViewAccessible GetNativeObject() override;
  void Destroy() override;

  // Supported IAccessible methods.

  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child) override;

  // Performs the object's default action.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id) override;

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left,
                           LONG* y_top,
                           LONG* width,
                           LONG* height,
                           VARIANT var_id) override;

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end) override;

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child) override;

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count) override;

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id,
                                    BSTR* default_action) override;

  // Retrieves the tooltip description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc) override;

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child) override;

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id,
                                       BSTR* access_key) override;

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name) override;

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent) override;

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role) override;

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state) override;

  // Retrieve or set the string value associated with the specified object.
  // Setting the value is not typically used by screen readers, but it's
  // used frequently by automation software.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value) override;
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR new_value) override;

  // Selections not applicable to views.
  STDMETHODIMP get_accSelection(VARIANT* selected) override;
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id) override;

  // Help functions not supported.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help) override;
  STDMETHODIMP get_accHelpTopic(BSTR* help_file,
                                VARIANT var_id,
                                LONG* topic_id) override;

  // Deprecated functions, not implemented here.
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name) override;

  //
  // IAccessible2
  //

  STDMETHODIMP role(LONG* role) override;

  STDMETHODIMP get_states(AccessibleStates* states) override;

  STDMETHODIMP get_uniqueID(LONG* unique_id) override;

  STDMETHODIMP get_windowHandle(HWND* window_handle) override;

  STDMETHODIMP get_relationTargetsOfType(BSTR type,
                                         long max_targets,
                                         IUnknown*** targets,
                                         long* n_targets) override;

  STDMETHODIMP get_attributes(BSTR* attributes) override;

  //
  // IAccessible2 methods not implemented.
  //

  STDMETHODIMP get_attribute(BSTR name, VARIANT* attribute) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_indexInParent(LONG* index_in_parent) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_extendedRole(BSTR* extended_role) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_nRelations(LONG* n_relations) override { return E_NOTIMPL; }
  STDMETHODIMP get_relation(LONG relation_index,
                            IAccessibleRelation** relation) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_relations(LONG max_relations,
                             IAccessibleRelation** relations,
                             LONG* n_relations) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollTo(enum IA2ScrollType scroll_type) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollToPoint(enum IA2CoordinateType coordinate_type,
                             LONG x,
                             LONG y) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_groupPosition(LONG* group_level,
                                 LONG* similar_items_in_group,
                                 LONG* position_in_group) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_localizedExtendedRole(
      BSTR* localized_extended_role) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_nExtendedStates(LONG* n_extended_states) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_extendedStates(LONG max_extended_states,
                                  BSTR** extended_states,
                                  LONG* n_extended_states) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_localizedExtendedStates(
      LONG max_localized_extended_states,
      BSTR** localized_extended_states,
      LONG* n_localized_extended_states) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_locale(IA2Locale* locale) override { return E_NOTIMPL; }
  STDMETHODIMP get_accessibleWithCaret(IUnknown** accessible,
                                       long* caret_offset) override {
    return E_NOTIMPL;
  }

  //
  // IAccessibleText methods.
  //

  STDMETHODIMP get_nCharacters(LONG* n_characters) override;

  STDMETHODIMP get_caretOffset(LONG* offset) override;

  STDMETHODIMP get_nSelections(LONG* n_selections) override;

  STDMETHODIMP get_selection(LONG selection_index,
                             LONG* start_offset,
                             LONG* end_offset) override;

  STDMETHODIMP get_text(LONG start_offset,
                        LONG end_offset,
                        BSTR* text) override;

  STDMETHODIMP get_textAtOffset(LONG offset,
                                enum IA2TextBoundaryType boundary_type,
                                LONG* start_offset,
                                LONG* end_offset,
                                BSTR* text) override;

  STDMETHODIMP get_textBeforeOffset(LONG offset,
                                    enum IA2TextBoundaryType boundary_type,
                                    LONG* start_offset,
                                    LONG* end_offset,
                                    BSTR* text) override;

  STDMETHODIMP get_textAfterOffset(LONG offset,
                                   enum IA2TextBoundaryType boundary_type,
                                   LONG* start_offset,
                                   LONG* end_offset,
                                   BSTR* text) override;

  STDMETHODIMP get_offsetAtPoint(LONG x,
                                 LONG y,
                                 enum IA2CoordinateType coord_type,
                                 LONG* offset) override;

  //
  // IAccessibleText methods not implemented.
  //

  STDMETHODIMP get_newText(IA2TextSegment* new_text) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_oldText(IA2TextSegment* old_text) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP addSelection(LONG start_offset, LONG end_offset) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_attributes(LONG offset,
                              LONG* start_offset,
                              LONG* end_offset,
                              BSTR* text_attributes) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_characterExtents(LONG offset,
                                    enum IA2CoordinateType coord_type,
                                    LONG* x,
                                    LONG* y,
                                    LONG* width,
                                    LONG* height) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP removeSelection(LONG selection_index) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP setCaretOffset(LONG offset) override { return E_NOTIMPL; }
  STDMETHODIMP setSelection(LONG selection_index,
                            LONG start_offset,
                            LONG end_offset) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollSubstringTo(LONG start_index,
                                 LONG end_index,
                                 enum IA2ScrollType scroll_type) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollSubstringToPoint(LONG start_index,
                                      LONG end_index,
                                      enum IA2CoordinateType coordinate_type,
                                      LONG x,
                                      LONG y) override {
    return E_NOTIMPL;
  }

  //
  // IServiceProvider methods.
  //

  STDMETHODIMP QueryService(REFGUID guidService,
                            REFIID riid,
                            void** object) override;

  //
  // IAccessibleEx methods not implemented.
  //
  STDMETHODIMP GetObjectForChild(long child_id, IAccessibleEx** ret) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP GetIAccessiblePair(IAccessible** acc, long* child_id) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP GetRuntimeId(SAFEARRAY** runtime_id) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP ConvertReturnedElement(IRawElementProviderSimple* element,
                                      IAccessibleEx** acc) override {
    return E_NOTIMPL;
  }

  //
  // IRawElementProviderSimple methods.
  //
  // The GetPatternProvider/GetPropertyValue methods need to be implemented for
  // the on-screen keyboard to show up in Windows 8 metro.
  STDMETHODIMP GetPatternProvider(PATTERNID id, IUnknown** provider) override;
  STDMETHODIMP GetPropertyValue(PROPERTYID id, VARIANT* ret) override;

  //
  // IRawElementProviderSimple methods not implemented.
  //
  STDMETHODIMP get_ProviderOptions(enum ProviderOptions* ret) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_HostRawElementProvider(
      IRawElementProviderSimple** provider) override {
    return E_NOTIMPL;
  }

  // Static methods

  // Returns a conversion from the event (as defined in ax_enums.idl)
  // to an MSAA event.
  static int32 MSAAEvent(ui::AXEvent event);

  // Returns a conversion from the Role (as defined in ax_enums.idl)
  // to an MSAA role.
  static int32 MSAARole(ui::AXRole role);

  // Returns a conversion from the State (as defined in ax_enums.idl)
  // to MSAA states set.
  static int32 MSAAState(const ui::AXViewState& state);

 protected:
  NativeViewAccessibilityWin();

 private:
  // Determines navigation direction for accNavigate, based on left, up and
  // previous being mapped all to previous and right, down, next being mapped
  // to next. Returns true if navigation direction is next, false otherwise.
  bool IsNavDirNext(int nav_dir) const;

  // Determines if the navigation target is within the allowed bounds. Returns
  // true if it is, false otherwise.
  bool IsValidNav(int nav_dir,
                  int start_id,
                  int lower_bound,
                  int upper_bound) const;

  // Determines if the child id variant is valid.
  bool IsValidId(const VARIANT& child) const;

  // Helper function which sets applicable states of view.
  void SetState(VARIANT* msaa_state, View* view);

  // Return the text to use for IAccessibleText.
  base::string16 TextForIAccessibleText();

  // If offset is a member of IA2TextSpecialOffsets this function updates the
  // value of offset and returns, otherwise offset remains unchanged.
  void HandleSpecialTextOffset(const base::string16& text, LONG* offset);

  // Convert from a IA2TextBoundaryType to a ui::TextBoundaryType.
  ui::TextBoundaryType IA2TextBoundaryToTextBoundary(IA2TextBoundaryType type);

  // Search forwards (direction == 1) or backwards (direction == -1)
  // from the given offset until the given boundary is found, and
  // return the offset of that boundary.
  LONG FindBoundary(const base::string16& text,
                    IA2TextBoundaryType ia2_boundary,
                    LONG start_offset,
                    ui::TextBoundaryDirection direction);

  // Populates the given vector with all widgets that are either a child
  // or are owned by this view's widget, and who are not contained in a
  // NativeViewHost.
  void PopulateChildWidgetVector(std::vector<Widget*>* child_widgets);

  // Adds this view to alert_target_view_storage_ids_.
  void AddAlertTarget();

  // Removes this view from alert_target_view_storage_ids_.
  void RemoveAlertTarget();

  // Give CComObject access to the class constructor.
  template <class Base> friend class CComObject;

  // A unique id for each object, needed for IAccessible2.
  long unique_id_;

  // Next unique id to assign.
  static long next_unique_id_;

  // Circular queue size.
  static const int kMaxViewStorageIds = 20;

  // Circular queue of view storage ids corresponding to child ids
  // used to post notifications using NotifyWinEvent.
  static int view_storage_ids_[kMaxViewStorageIds];

  // Next index into |view_storage_ids_| to use.
  static int next_view_storage_id_index_;

  // A vector of view storage ids of views that have been the target of
  // an alert event, in order to provide an api to quickly identify all
  // open alerts.
  static std::vector<int> alert_target_view_storage_ids_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibilityWin);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_WIN_H_
