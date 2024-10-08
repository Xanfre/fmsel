// generated by Fast Light User Interface Designer (fluid) version 1.0309

#include "fl_taged.h"

Fl_FM_TagEd_Window *pTagEdWnd=(Fl_FM_TagEd_Window *)0;

static Fl_Tabs *pTagEdTabs=(Fl_Tabs *)0;

static Fl_Input *pTagEdNiceName=(Fl_Input *)0;

static Fl_Output *pTagEdName=(Fl_Output *)0;

static Fl_Output *pTagEdArchive=(Fl_Output *)0;

static Fl_Choice *pTagEdInfoFile=(Fl_Choice *)0;

static Fl_Button *pTagEdViewInfo=(Fl_Button *)0;

static Fl_Int_Input *pTagEdRelYear=(Fl_Int_Input *)0;

static Fl_Int_Input *pTagEdRelMonth=(Fl_Int_Input *)0;

static Fl_Int_Input *pTagEdRelDay=(Fl_Int_Input *)0;

static Fl_Check_Button *pTagEdUnverified=(Fl_Check_Button *)0;

static Fl_Input *pTagEdModExclude=(Fl_Input *)0;

static Fl_FM_Tag_Input *pTagEdInput=(Fl_FM_Tag_Input *)0;

Fl_Button *pTagEdAddTag=(Fl_Button *)0;

static Fl_Choice *pTagEdPresets=(Fl_Choice *)0;

Fl_FM_Tag_Group *pTagEdGroup=(Fl_FM_Tag_Group *)0;

static Fl_Text_Editor *pTagEdDescr=(Fl_Text_Editor *)0;

static Fl_Text_Editor *pTagEdNotes=(Fl_Text_Editor *)0;

static Fl_FM_TagEd_Window* MakeTagEditor(int scale) {
  { pTagEdWnd = new Fl_FM_TagEd_Window(SCALE(540), SCALE(260), $("FMSel"));
    pTagEdWnd->box(FL_FLAT_BOX);
    pTagEdWnd->color(FL_BACKGROUND_COLOR);
    pTagEdWnd->selection_color(FL_BACKGROUND_COLOR);
    pTagEdWnd->labeltype(FL_NO_LABEL);
    pTagEdWnd->labelfont(0);
    pTagEdWnd->labelsize(SCALE(14));
    pTagEdWnd->labelcolor(FL_FOREGROUND_COLOR);
    pTagEdWnd->align(Fl_Align(FL_ALIGN_TOP));
    pTagEdWnd->when(FL_WHEN_RELEASE);
    { pTagEdTabs = new Fl_Tabs(SCALE(0), SCALE(4), SCALE(544), SCALE(215));
      { Fl_Group* o = new Fl_Group(SCALE(0), SCALE(22), SCALE(540), SCALE(197), $("Misc"));
        o->labelfont(1);
        o->labelsize(SCALE(11));
        o->labelcolor(FL_INACTIVE_COLOR);
        o->hide();
        { Fl_Group* o = new Fl_Group(SCALE(6), SCALE(29), SCALE(528), SCALE(134));
          { pTagEdNiceName = new Fl_Input(SCALE(6), SCALE(43), SCALE(250), SCALE(25), $("Display Name"));
            pTagEdNiceName->tooltip($("Optional name to display in list instead of directory name"));
            pTagEdNiceName->labelsize(SCALE(11));
            pTagEdNiceName->align(Fl_Align(FL_ALIGN_TOP_LEFT));
            pTagEdNiceName->when(FL_WHEN_CHANGED);
            Fl_Group::current()->resizable(pTagEdNiceName);
          } // Fl_Input* pTagEdNiceName
          { Fl_Output* o = pTagEdName = new Fl_Output(SCALE(6), SCALE(90), SCALE(250), SCALE(25), $("Directory Name"));
            pTagEdName->tooltip($("Directory name (default name displayed in list)"));
            pTagEdName->labelsize(SCALE(11));
            pTagEdName->align(Fl_Align(FL_ALIGN_TOP_LEFT));
            o->color(FL_GRAY0+19);
          } // Fl_Output* pTagEdName
          { Fl_Output* o = pTagEdArchive = new Fl_Output(SCALE(6), SCALE(137), SCALE(250), SCALE(25), $("Archive  Name"));
            pTagEdArchive->tooltip($("Archive file associated with this FM"));
            pTagEdArchive->labelsize(SCALE(11));
            pTagEdArchive->align(Fl_Align(FL_ALIGN_TOP_LEFT));
            o->color(FL_GRAY0+19);
          } // Fl_Output* pTagEdArchive
          { pTagEdInfoFile = new Fl_Choice(SCALE(278), SCALE(43), SCALE(225), SCALE(25), $("Info File"));
            pTagEdInfoFile->tooltip($("FM info/readme file"));
            pTagEdInfoFile->down_box(FL_BORDER_BOX);
            pTagEdInfoFile->labelsize(SCALE(11));
            pTagEdInfoFile->align(Fl_Align(FL_ALIGN_TOP_LEFT));
          } // Fl_Choice* pTagEdInfoFile
          { Fl_Button* o = pTagEdViewInfo = new Fl_Button(SCALE(508), SCALE(43), SCALE(26), SCALE(25));
            pTagEdViewInfo->tooltip($("View/Open info file"));
            pTagEdViewInfo->labelsize(SCALE(12));
            pTagEdViewInfo->callback((Fl_Callback*)OnTagEdViewInfoFile);
            pTagEdViewInfo->align(Fl_Align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
            o->image(imgViewFile);
            o->deimage(pImgViewFileGray);
          } // Fl_Button* pTagEdViewInfo
          { Fl_Box* o = new Fl_Box(SCALE(276), SCALE(76), SCALE(115), SCALE(16), $("Release Date"));
            o->labelsize(SCALE(11));
            o->align(Fl_Align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE));
          } // Fl_Box* o
          { pTagEdRelYear = new Fl_Int_Input(SCALE(278), SCALE(90), SCALE(40), SCALE(25), $("Year"));
            pTagEdRelYear->tooltip($("Release date year (YYYY)"));
            pTagEdRelYear->type(2);
            pTagEdRelYear->labelsize(SCALE(10));
            pTagEdRelYear->align(Fl_Align(FL_ALIGN_BOTTOM));
            pTagEdRelYear->when(FL_WHEN_CHANGED);
          } // Fl_Int_Input* pTagEdRelYear
          { new Fl_Box(SCALE(320), SCALE(90), SCALE(10), SCALE(25), $("-"));
          } // Fl_Box* o
          { pTagEdRelMonth = new Fl_Int_Input(SCALE(331), SCALE(91), SCALE(25), SCALE(24), $("Month"));
            pTagEdRelMonth->tooltip($("Release date month (1-12)"));
            pTagEdRelMonth->type(2);
            pTagEdRelMonth->labelsize(SCALE(10));
            pTagEdRelMonth->align(Fl_Align(FL_ALIGN_BOTTOM));
            pTagEdRelMonth->when(FL_WHEN_CHANGED);
          } // Fl_Int_Input* pTagEdRelMonth
          { new Fl_Box(SCALE(358), SCALE(90), SCALE(10), SCALE(25), $("-"));
          } // Fl_Box* o
          { pTagEdRelDay = new Fl_Int_Input(SCALE(368), SCALE(90), SCALE(25), SCALE(25), $("Day"));
            pTagEdRelDay->tooltip($("Release date day (1-31)"));
            pTagEdRelDay->type(2);
            pTagEdRelDay->labelsize(SCALE(10));
            pTagEdRelDay->align(Fl_Align(FL_ALIGN_BOTTOM));
            pTagEdRelDay->when(FL_WHEN_CHANGED);
          } // Fl_Int_Input* pTagEdRelDay
          { pTagEdUnverified = new Fl_Check_Button(SCALE(402), SCALE(92), SCALE(125), SCALE(21), $("Unverified"));
            pTagEdUnverified->tooltip($("Release date is unverified (usually set for automatically scanned date)"));
            pTagEdUnverified->down_box(FL_DOWN_BOX);
          } // Fl_Check_Button* pTagEdUnverified
          o->end();
        } // Fl_Group* o
        { Fl_Group* o = new Fl_Group(SCALE(6), SCALE(164), SCALE(528), SCALE(4));
          o->end();
          Fl_Group::current()->resizable(o);
        } // Fl_Group* o
        { pTagEdModExclude = new Fl_Input(SCALE(6), SCALE(183), SCALE(528), SCALE(25), $("Mod Path Excludes"));
          pTagEdModExclude->tooltip($("Mod paths to exclude when running this FM (use + to separate paths)"));
          pTagEdModExclude->labelsize(SCALE(11));
          pTagEdModExclude->align(Fl_Align(FL_ALIGN_TOP_LEFT));
          pTagEdModExclude->when(FL_WHEN_CHANGED);
        } // Fl_Input* pTagEdModExclude
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
        Fl_Group::current()->resizable(o);
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(SCALE(0), SCALE(22), SCALE(544), SCALE(197), $("Tags"));
        o->labelfont(1);
        o->labelsize(SCALE(11));
        o->labelcolor(FL_INACTIVE_COLOR);
        o->hide();
        { Fl_Group* o = new Fl_Group(SCALE(6), SCALE(28), SCALE(528), SCALE(45));
          { Fl_FM_Tag_Input* o = pTagEdInput = new Fl_FM_Tag_Input(SCALE(6), SCALE(43), SCALE(242), SCALE(25), $("Add Tag"));
            pTagEdInput->tooltip($("Press enter to add tag"));
            pTagEdInput->box(FL_DOWN_BOX);
            pTagEdInput->color(FL_BACKGROUND2_COLOR);
            pTagEdInput->selection_color(FL_SELECTION_COLOR);
            pTagEdInput->labeltype(FL_NORMAL_LABEL);
            pTagEdInput->labelfont(0);
            pTagEdInput->labelsize(SCALE(11));
            pTagEdInput->labelcolor(FL_FOREGROUND_COLOR);
            pTagEdInput->textsize(SCALE(12));
            pTagEdInput->align(Fl_Align(FL_ALIGN_TOP_LEFT));
            pTagEdInput->when(FL_WHEN_CHANGED);
            o->tag_callback((Fl_Callback*)OnAddCustomTag);
            o->textsize(FL_NORMAL_SIZE);
          } // Fl_FM_Tag_Input* pTagEdInput
          { Fl_Button* o = pTagEdAddTag = new Fl_Button(SCALE(251), SCALE(43), SCALE(26), SCALE(25));
            pTagEdAddTag->tooltip($("Add Tag"));
            pTagEdAddTag->labelsize(SCALE(12));
            pTagEdAddTag->callback((Fl_Callback*)OnTagEdAddTag);
            pTagEdAddTag->align(Fl_Align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
            o->image(imgReturn);
            o->deimage(imgReturnGray);
          } // Fl_Button* pTagEdAddTag
          { Fl_Group* o = new Fl_Group(SCALE(283), SCALE(28), SCALE(96), SCALE(33));
            o->hide();
            o->end();
            Fl_Group::current()->resizable(o);
          } // Fl_Group* o
          { Fl_Choice* o = pTagEdPresets = new Fl_Choice(SCALE(386), SCALE(43), SCALE(147), SCALE(25));
            pTagEdPresets->down_box(FL_BORDER_BOX);
            pTagEdPresets->labelsize(SCALE(11));
            pTagEdPresets->textsize(SCALE(12));
            pTagEdPresets->callback((Fl_Callback*)OnTagPreset);
            pTagEdPresets->align(Fl_Align(FL_ALIGN_TOP_LEFT));
            o->menu(g_mnuTagPresets);
          } // Fl_Choice* pTagEdPresets
          o->end();
        } // Fl_Group* o
        { Fl_FM_Tag_Group* o = pTagEdGroup = new Fl_FM_Tag_Group(SCALE(6), SCALE(78), SCALE(528), SCALE(134));
          pTagEdGroup->box(FL_FLAT_BOX);
          pTagEdGroup->color((Fl_Color)16);
          pTagEdGroup->selection_color(FL_BACKGROUND_COLOR);
          pTagEdGroup->labeltype(FL_NORMAL_LABEL);
          pTagEdGroup->labelfont(0);
          pTagEdGroup->labelsize(SCALE(14));
          pTagEdGroup->labelcolor(FL_FOREGROUND_COLOR);
          pTagEdGroup->align(Fl_Align(FL_ALIGN_TOP));
          pTagEdGroup->when(FL_WHEN_RELEASE);
          o->color((Fl_Color)(USER_CLR+28));
          pTagEdGroup->end();
          Fl_Group::current()->resizable(pTagEdGroup);
        } // Fl_FM_Tag_Group* pTagEdGroup
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(SCALE(0), SCALE(22), SCALE(540), SCALE(197), $("Descr"));
        o->labelfont(1);
        o->labelsize(SCALE(11));
        o->labelcolor(FL_INACTIVE_COLOR);
        o->hide();
        { pTagEdDescr = new Fl_Text_Editor(SCALE(6), SCALE(30), SCALE(528), SCALE(182));
          pTagEdDescr->tooltip($("Mission description with max 1500 characters"));
          pTagEdDescr->box(FL_FLAT_BOX);
          pTagEdDescr->labelsize(SCALE(12));
          Fl_Group::current()->resizable(pTagEdDescr);
        } // Fl_Text_Editor* pTagEdDescr
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(SCALE(0), SCALE(22), SCALE(540), SCALE(197), $("Notes"));
        o->labelfont(1);
        o->labelsize(SCALE(11));
        o->labelcolor(FL_INACTIVE_COLOR);
        { pTagEdNotes = new Fl_Text_Editor(SCALE(6), SCALE(30), SCALE(528), SCALE(182));
          pTagEdNotes->box(FL_FLAT_BOX);
          pTagEdNotes->labelsize(SCALE(12));
          Fl_Group::current()->resizable(pTagEdNotes);
        } // Fl_Text_Editor* pTagEdNotes
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
      } // Fl_Group* o
      pTagEdTabs->end();
      Fl_Group::current()->resizable(pTagEdTabs);
    } // Fl_Tabs* pTagEdTabs
    { Fl_Group* o = new Fl_Group(SCALE(5), SCALE(227), SCALE(533), SCALE(25));
      { Fl_Button* o = new Fl_Button(SCALE(448), SCALE(227), SCALE(80), SCALE(25), $("Cancel"));
        o->shortcut(0xff1b);
        o->callback((Fl_Callback*)OnCancelTagEditor);
      } // Fl_Button* o
      { Fl_Button* o = new Fl_Button(SCALE(357), SCALE(227), SCALE(80), SCALE(25), $("OK"));
        o->callback((Fl_Callback*)OnOkTagEditor);
      } // Fl_Button* o
      { Fl_Group* o = new Fl_Group(SCALE(5), SCALE(227), SCALE(345), SCALE(25));
        o->end();
        Fl_Group::current()->resizable(o);
      } // Fl_Group* o
      o->end();
    } // Fl_Group* o
    pTagEdWnd->set_modal();
    pTagEdWnd->size_range(SCALE(540), SCALE(260));
    pTagEdWnd->end();
  } // Fl_FM_TagEd_Window* pTagEdWnd
  return pTagEdWnd;
}
