// generated by Fast Light User Interface Designer (fluid) version 1.0110

#include "fl_taged.h"

Fl_FM_TagEd_Window *pTagEdWnd=(Fl_FM_TagEd_Window *)0;

static Fl_Tabs *pTagEdTabs=(Fl_Tabs *)0;

static Fl_Input *pTagEdNiceName=(Fl_Input *)0;

static Fl_Output *pTagEdName=(Fl_Output *)0;

static Fl_Output *pTagEdArchive=(Fl_Output *)0;

static Fl_Choice *pTagEdInfoFile=(Fl_Choice *)0;

static Fl_Button *pTagEdViewInfo=(Fl_Button *)0;

static Fl_Input *pTagEdRelYear=(Fl_Input *)0;

static Fl_Input *pTagEdRelMonth=(Fl_Input *)0;

static Fl_Input *pTagEdRelDay=(Fl_Input *)0;

static Fl_Check_Button *pTagEdUnverified=(Fl_Check_Button *)0;

static Fl_Input *pTagEdModExclude=(Fl_Input *)0;

static Fl_FM_Tag_Input *pTagEdInput=(Fl_FM_Tag_Input *)0;

Fl_Button *pTagEdAddTag=(Fl_Button *)0;

static Fl_Choice *pTagEdPresets=(Fl_Choice *)0;

Fl_FM_Tag_Group *pTagEdGroup=(Fl_FM_Tag_Group *)0;

static Fl_Text_Editor *pTagEdDescr=(Fl_Text_Editor *)0;

static Fl_Text_Editor *pTagEdNotes=(Fl_Text_Editor *)0;

static Fl_FM_TagEd_Window* MakeTagEditor() {
  { pTagEdWnd = new Fl_FM_TagEd_Window(540, 260, $("FMSel"));
    pTagEdWnd->box(FL_FLAT_BOX);
    pTagEdWnd->color((Fl_Color)FL_BACKGROUND_COLOR);
    pTagEdWnd->selection_color((Fl_Color)FL_BACKGROUND_COLOR);
    pTagEdWnd->labeltype(FL_NO_LABEL);
    pTagEdWnd->labelfont(0);
    pTagEdWnd->labelsize(14);
    pTagEdWnd->labelcolor((Fl_Color)FL_FOREGROUND_COLOR);
    pTagEdWnd->align(FL_ALIGN_TOP);
    pTagEdWnd->when(FL_WHEN_RELEASE);
    { pTagEdTabs = new Fl_Tabs(0, 4, 544, 215);
      { Fl_Group* o = new Fl_Group(0, 22, 540, 197, $("Misc"));
        o->labelfont(1);
        o->labelsize(11);
        o->labelcolor((Fl_Color)FL_INACTIVE_COLOR);
        { Fl_Group* o = new Fl_Group(6, 29, 528, 134);
          { pTagEdNiceName = new Fl_Input(6, 43, 250, 25, $("Display Name"));
            pTagEdNiceName->tooltip($("Optional name to display in list instead of directory name"));
            pTagEdNiceName->labelsize(11);
            pTagEdNiceName->align(FL_ALIGN_TOP_LEFT);
            pTagEdNiceName->when(FL_WHEN_CHANGED);
            Fl_Group::current()->resizable(pTagEdNiceName);
          } // Fl_Input* pTagEdNiceName
          { Fl_Output* o = pTagEdName = new Fl_Output(6, 90, 250, 25, $("Directory Name"));
            pTagEdName->tooltip($("Directory name (default name displayed in list)"));
            pTagEdName->labelsize(11);
            pTagEdName->align(FL_ALIGN_TOP_LEFT);
            o->color(FL_GRAY0+19);
          } // Fl_Output* pTagEdName
          { Fl_Output* o = pTagEdArchive = new Fl_Output(6, 137, 250, 25, $("Archive  Name"));
            pTagEdArchive->tooltip($("Archive file associated with this FM"));
            pTagEdArchive->labelsize(11);
            pTagEdArchive->align(FL_ALIGN_TOP_LEFT);
            o->color(FL_GRAY0+19);
          } // Fl_Output* pTagEdArchive
          { pTagEdInfoFile = new Fl_Choice(278, 43, 225, 25, $("Info File"));
            pTagEdInfoFile->tooltip($("FM info/readme file"));
            pTagEdInfoFile->down_box(FL_BORDER_BOX);
            pTagEdInfoFile->labelsize(11);
            pTagEdInfoFile->align(FL_ALIGN_TOP_LEFT);
          } // Fl_Choice* pTagEdInfoFile
          { Fl_Button* o = pTagEdViewInfo = new Fl_Button(508, 43, 26, 25);
            pTagEdViewInfo->tooltip($("View/Open info file"));
            pTagEdViewInfo->labelsize(12);
            pTagEdViewInfo->callback((Fl_Callback*)OnTagEdViewInfoFile);
            pTagEdViewInfo->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
            o->image(imgViewFile);
            o->deimage(pImgViewFileGray);
          } // Fl_Button* pTagEdViewInfo
          { Fl_Box* o = new Fl_Box(276, 76, 115, 16, $("Release Date"));
            o->labelsize(11);
            o->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
          } // Fl_Box* o
          { pTagEdRelYear = new Fl_Input(278, 90, 40, 25, $("Year"));
            pTagEdRelYear->tooltip($("Release date year (YYYY)"));
            pTagEdRelYear->type(2);
            pTagEdRelYear->labelsize(10);
            pTagEdRelYear->align(FL_ALIGN_BOTTOM);
            pTagEdRelYear->when(FL_WHEN_CHANGED);
          } // Fl_Input* pTagEdRelYear
          { new Fl_Box(320, 90, 10, 25, $("-"));
          } // Fl_Box* o
          { pTagEdRelMonth = new Fl_Input(331, 91, 25, 24, $("Month"));
            pTagEdRelMonth->tooltip($("Release date month (1-12)"));
            pTagEdRelMonth->type(2);
            pTagEdRelMonth->labelsize(10);
            pTagEdRelMonth->align(FL_ALIGN_BOTTOM);
            pTagEdRelMonth->when(FL_WHEN_CHANGED);
          } // Fl_Input* pTagEdRelMonth
          { new Fl_Box(358, 90, 10, 25, $("-"));
          } // Fl_Box* o
          { pTagEdRelDay = new Fl_Input(368, 90, 25, 25, $("Day"));
            pTagEdRelDay->tooltip($("Release date day (1-31)"));
            pTagEdRelDay->type(2);
            pTagEdRelDay->labelsize(10);
            pTagEdRelDay->align(FL_ALIGN_BOTTOM);
            pTagEdRelDay->when(FL_WHEN_CHANGED);
          } // Fl_Input* pTagEdRelDay
          { pTagEdUnverified = new Fl_Check_Button(402, 92, 125, 21, $("Unverified"));
            pTagEdUnverified->tooltip($("Release date is unverified (usually set for automatically scanned date)"));
            pTagEdUnverified->down_box(FL_DOWN_BOX);
          } // Fl_Check_Button* pTagEdUnverified
          o->end();
        } // Fl_Group* o
        { Fl_Group* o = new Fl_Group(6, 164, 528, 4);
          o->end();
          Fl_Group::current()->resizable(o);
        } // Fl_Group* o
        { pTagEdModExclude = new Fl_Input(6, 183, 528, 25, $("Mod Path Excludes"));
          pTagEdModExclude->tooltip($("Mod paths to exclude when running this FM (use + to separate paths)"));
          pTagEdModExclude->labelsize(11);
          pTagEdModExclude->align(FL_ALIGN_TOP_LEFT);
          pTagEdModExclude->when(FL_WHEN_CHANGED);
        } // Fl_Input* pTagEdModExclude
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
        Fl_Group::current()->resizable(o);
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(0, 22, 544, 197, $("Tags"));
        o->labelfont(1);
        o->labelsize(11);
        o->labelcolor((Fl_Color)FL_INACTIVE_COLOR);
        o->hide();
        { Fl_Group* o = new Fl_Group(6, 28, 528, 45);
          { Fl_FM_Tag_Input* o = pTagEdInput = new Fl_FM_Tag_Input(6, 43, 242, 25, $("Add Tag"));
            pTagEdInput->tooltip($("Press enter to add tag"));
            pTagEdInput->box(FL_DOWN_BOX);
            pTagEdInput->color((Fl_Color)FL_BACKGROUND2_COLOR);
            pTagEdInput->selection_color((Fl_Color)FL_SELECTION_COLOR);
            pTagEdInput->labeltype(FL_NORMAL_LABEL);
            pTagEdInput->labelfont(0);
            pTagEdInput->labelsize(11);
            pTagEdInput->labelcolor((Fl_Color)FL_FOREGROUND_COLOR);
            pTagEdInput->textsize(12);
            pTagEdInput->align(FL_ALIGN_TOP_LEFT);
            pTagEdInput->when(FL_WHEN_CHANGED);
            o->tag_callback((Fl_Callback*)OnAddCustomTag);
            o->textsize(FL_NORMAL_SIZE);
          } // Fl_FM_Tag_Input* pTagEdInput
          { Fl_Button* o = pTagEdAddTag = new Fl_Button(251, 43, 26, 25);
            pTagEdAddTag->tooltip($("Add Tag"));
            pTagEdAddTag->labelsize(12);
            pTagEdAddTag->callback((Fl_Callback*)OnTagEdAddTag);
            pTagEdAddTag->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
            o->image(imgReturn);
            o->deimage(imgReturnGray);
          } // Fl_Button* pTagEdAddTag
          { Fl_Group* o = new Fl_Group(283, 28, 96, 33);
            o->hide();
            o->end();
            Fl_Group::current()->resizable(o);
          } // Fl_Group* o
          { Fl_Choice* o = pTagEdPresets = new Fl_Choice(386, 43, 147, 25);
            pTagEdPresets->down_box(FL_BORDER_BOX);
            pTagEdPresets->labelsize(11);
            pTagEdPresets->textsize(12);
            pTagEdPresets->callback((Fl_Callback*)OnTagPreset);
            pTagEdPresets->align(FL_ALIGN_TOP_LEFT);
            o->menu(g_mnuTagPresets);
          } // Fl_Choice* pTagEdPresets
          o->end();
        } // Fl_Group* o
        { Fl_FM_Tag_Group* o = pTagEdGroup = new Fl_FM_Tag_Group(6, 78, 528, 134);
          pTagEdGroup->box(FL_FLAT_BOX);
          pTagEdGroup->color((Fl_Color)16);
          pTagEdGroup->selection_color((Fl_Color)FL_BACKGROUND_COLOR);
          pTagEdGroup->labeltype(FL_NORMAL_LABEL);
          pTagEdGroup->labelfont(0);
          pTagEdGroup->labelsize(14);
          pTagEdGroup->labelcolor((Fl_Color)FL_FOREGROUND_COLOR);
          pTagEdGroup->align(FL_ALIGN_TOP);
          pTagEdGroup->when(FL_WHEN_RELEASE);
          o->color( fl_rgb_color(226,226,217) );
          pTagEdGroup->end();
          Fl_Group::current()->resizable(pTagEdGroup);
        } // Fl_FM_Tag_Group* pTagEdGroup
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(0, 22, 540, 197, $("Descr"));
        o->labelfont(1);
        o->labelsize(11);
        o->labelcolor((Fl_Color)FL_INACTIVE_COLOR);
        o->hide();
        { pTagEdDescr = new Fl_Text_Editor(6, 30, 528, 182);
          pTagEdDescr->tooltip($("Mission description with max 1500 characters"));
          pTagEdDescr->box(FL_FLAT_BOX);
          pTagEdDescr->labelsize(12);
          Fl_Group::current()->resizable(pTagEdDescr);
        } // Fl_Text_Editor* pTagEdDescr
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(0, 22, 540, 197, $("Notes"));
        o->labelfont(1);
        o->labelsize(11);
        o->labelcolor((Fl_Color)FL_INACTIVE_COLOR);
        o->hide();
        { pTagEdNotes = new Fl_Text_Editor(6, 30, 528, 182);
          pTagEdNotes->box(FL_FLAT_BOX);
          pTagEdNotes->labelsize(12);
          Fl_Group::current()->resizable(pTagEdNotes);
        } // Fl_Text_Editor* pTagEdNotes
        o->labelsize(FL_NORMAL_SIZE-1);
        o->end();
      } // Fl_Group* o
      pTagEdTabs->end();
      Fl_Group::current()->resizable(pTagEdTabs);
    } // Fl_Tabs* pTagEdTabs
    { Fl_Group* o = new Fl_Group(5, 227, 533, 25);
      { Fl_Button* o = new Fl_Button(448, 227, 80, 25, $("Cancel"));
        o->shortcut(0xff1b);
        o->callback((Fl_Callback*)OnCancelTagEditor);
      } // Fl_Button* o
      { Fl_Button* o = new Fl_Button(357, 227, 80, 25, $("OK"));
        o->callback((Fl_Callback*)OnOkTagEditor);
      } // Fl_Button* o
      { Fl_Group* o = new Fl_Group(5, 227, 345, 25);
        o->end();
        Fl_Group::current()->resizable(o);
      } // Fl_Group* o
      o->end();
    } // Fl_Group* o
    pTagEdWnd->set_modal();
    pTagEdWnd->size_range(540, 260);
    pTagEdWnd->end();
  } // Fl_FM_TagEd_Window* pTagEdWnd
  return pTagEdWnd;
}