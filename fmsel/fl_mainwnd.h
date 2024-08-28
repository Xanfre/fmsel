// generated by Fast Light User Interface Designer (fluid) version 1.0309

#ifndef fl_mainwnd_h
#define fl_mainwnd_h
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
extern void OnClose(Fl_Double_Window*, void*);
extern Fl_Double_Window *pMainWnd;
extern Fl_FM_List *pFMList;
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
extern void OnPlayOM(Fl_Button*, void*);
extern void OnPlayFM(Fl_FM_Return_Button*, void*);
extern void OnStartFM(Fl_Button*, void*);
extern void OnExit(Fl_Button*, void*);
extern void OnFilterName(Fl_FM_Filter_Input*, void*);
#include <FL/Fl_Choice.H>
extern void OnFilterRating(Fl_Choice*, void*);
extern void OnFilterPriority(Fl_Choice*, void*);
#include <FL/Fl_Box.H>
extern void OnFilterToggle(Fl_FM_Filter_Button*, void*);
extern void OnFilterRelFrom(Fl_FM_Filter_Input*, void*);
extern void OnFilterRelTo(Fl_FM_Filter_Input*, void*);
extern void OnRefreshFilters(Fl_Button*, void*);
extern void OnResetFilters(Fl_Button*, void*);
#endif
