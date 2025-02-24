#include <wx/wx.h>
#include <hunspell/hunspell.hxx>

class MyApp : public wxApp {
public:
    virtual bool OnInit();
};

class MyFrame : public wxFrame {
public:
    MyFrame() : wxFrame(NULL, wxID_ANY, "Kalahari - Book Writers Asistant - 1.0.0") {}
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit() {
    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}
