#include "NativeAnalysisDialog.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "wxExtensions.hpp"
#include "Jobs/Worker.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/Nonplanar/NativeAnalysisView.hpp"
#include "libslic3r/Nonplanar/StlFile.hpp"
#include <boost/filesystem/path.hpp>
#include <nlohmann/json.hpp>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/slider.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/weakref.h>
#include <atomic>
#include <set>
#include <limits>

namespace Slic3r::GUI {
namespace {
using namespace nptop;
using Json = nlohmann::json;
struct AnalysisRun {
    Model model;
    DynamicPrintConfig config;
    int plate;
    Vec3d origin;
    uint64_t revision;
    std::shared_ptr<const NativeAnalysisViewInput> input;
    std::unique_ptr<Print> print;
    std::shared_ptr<const GuardedJobTask> task;
    NativeAnalysisWorkerResult result;
    std::atomic<bool> cancelled{false};
    std::atomic<int> stage{-1};
    AnalysisRun(const Model &m, NativeAnalysisHostState state, uint64_t r,
        std::shared_ptr<const NativeAnalysisViewInput> i)
        : model(m), config(std::move(state.config)), plate(state.plate), origin(state.origin), revision(r), input(std::move(i)) {}
    void process(Job::Ctl &ctl, const std::string &executable)
    {
        const auto started = std::chrono::steady_clock::now();
        const auto cancelled_now = [&] { return cancelled.load(std::memory_order_relaxed) || ctl.was_canceled(); };
        const auto remaining = [&] { return std::chrono::milliseconds(30000) -
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started); };
        try {
            config.set_key_value("nptop_mode", new ConfigOptionString(input->mode));
            if (const auto conflict = input_policy_conflict(model, config)) {
                result.reason = "VIEW_POLICY_REFUSED:" + conflict->key; return;
            }
            std::vector<JobResource> files{{JobResourceKind::SourceFile, "native-analysis-json-v1", input->editing_bytes},
                {JobResourceKind::SourceFile, "native-analysis-request-v1", input->request->canonical_json}};
            std::set<std::string> paths;
            for (const auto *object : model.objects) for (const auto *volume : object->volumes)
                if (!volume->source.input_file.empty()) paths.insert(volume->source.input_file);
            require(paths.size() <= 247, "VIEW_SOURCE_COUNT");
            size_t bytes = input->editing_bytes.size() + input->request->canonical_json.size();
            for (const auto &path : paths) {
                require(remaining().count() > 0, "VIEW_TIMEOUT");
                StlFileOptions io; io.cancelled = cancelled_now; io.timeout = std::min(remaining(), std::chrono::milliseconds(1000));
                const auto source = capture_stl_file(path, true, revision, io);
                require(bool(source.source), "VIEW_SOURCE_FILE_REFUSED");
                require(bytes <= 16 * 1024 * 1024 && source.source->bytes.size() <= 16 * 1024 * 1024 - bytes, "VIEW_SOURCE_BYTES");
                bytes += source.source->bytes.size(); files.push_back({JobResourceKind::SourceFile, path, source.source->bytes});
            }
            require(!cancelled_now() && remaining().count() > 0, "VIEW_CANCELLED_OR_TIMEOUT");
            print = std::make_unique<Print>(); print->set_plate_index(plate); print->set_plate_origin(origin);
            print->apply(model, config);
            // Report the actual engine override responsible for a refusal.
            // Do not display imported hook bodies or accept weaker settings.
            const auto settings = capture_print_config(*print);
            require(bool(settings), "VIEW_PRINT_CONFIG_REFUSED");
            const auto policy = resolve_policy(settings->resolved_print_config, settings->object_count, settings->instance_count);
            if (!policy.conflicts.empty()) {
                result.reason = "VIEW_CONFIG_POLICY_REFUSED:" + policy.conflicts.front().key; return;
            }
            for (const auto &region : settings->regions) if (!region.policy.conflicts.empty()) {
                result.reason = "VIEW_REGION_POLICY_REFUSED:" + region.policy.conflicts.front().key; return;
            }
            GuardedJobLimits capture; capture.cancelled = cancelled_now;
            capture.timeout = std::min(remaining(), std::chrono::milliseconds(1000));
            const auto job = begin_guarded_job(*print, revision, files, capture, JobSoftwareMode::CompiledInputs, &input->request->values.inputs);
            result.reason = job.reason; task = job.task;
            if (!task) return;
            const auto worker = capture_native_analysis_worker_input(task, input->request);
            NativeAnalysisWorkerOptions limits; limits.cancelled = cancelled_now; limits.timeout = remaining();
            limits.progress = [&](NativeAnalysisStage s) {
                stage.store(int(s), std::memory_order_relaxed);
                ctl.update_status(10 * (1 + int(s)), std::string("Nonplanar analysis: ") + native_analysis_stage_name(s));
            };
            result = run_native_analysis_worker(executable, worker, limits);
        } catch (const std::exception &) { result = {"VIEW_INPUT_REFUSED", {}}; }
        catch (...) { result = {"VIEW_INPUT_EXCEPTION", {}}; }
    }
};

class ReplayCanvas : public wxPanel {
public:
    Json moves = Json::array();
    size_t selected = 0;
    int projection = 1;
    explicit ReplayCanvas(wxWindow *parent) : wxPanel(parent)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, [this](wxPaintEvent &) { paint(); });
    }
private:
    void paint()
    {
        wxAutoBufferedPaintDC dc(this); dc.SetBackground(wxBrush(GetBackgroundColour())); dc.Clear();
        if (moves.empty()) return;
        const int a = projection == 2 ? 1 : 0, b = projection == 0 ? 1 : 2;
        double min_a = std::numeric_limits<double>::max(), max_a = -min_a, min_b = min_a, max_b = -min_a;
        for (const auto &m : moves) for (const char *key : {"start_mm", "end_mm"}) {
            const auto &p = m.at(key); min_a = std::min(min_a, p[a].get<double>()); max_a = std::max(max_a, p[a].get<double>());
            min_b = std::min(min_b, p[b].get<double>()); max_b = std::max(max_b, p[b].get<double>());
        }
        const auto size = GetClientSize();
        const double scale = std::min(std::max(1, size.x - 40) / std::max(.001, max_a - min_a),
            std::max(1, size.y - 40) / std::max(.001, max_b - min_b));
        const auto pixel = [&](const Json &p) { return wxPoint(20 + int((p[a].get<double>() - min_a) * scale),
            size.y - 20 - int((p[b].get<double>() - min_b) * scale)); };
        // Preview-only LOD. The selected original movement is always shown.
        const size_t stride = std::max<size_t>(1, (moves.size() + 9999) / 10000);
        for (size_t i = 0; i <= std::min(selected, moves.size() - 1); i += stride) {
            const auto &m = moves[i]; dc.SetPen(wxPen(m.at("kind") == 0 ? wxColour(25, 125, 190) : wxColour(150, 150, 150)));
            dc.DrawLine(pixel(m.at("start_mm")), pixel(m.at("end_mm")));
        }
        const auto &m = moves.at(selected); dc.SetPen(wxPen(wxColour(225, 90, 20), 3));
        dc.DrawLine(pixel(m.at("start_mm")), pixel(m.at("end_mm")));
        dc.SetBrush(wxBrush(wxColour(225, 90, 20))); dc.DrawCircle(pixel(m.at("end_mm")), 4);
    }
};

class NativeAnalysisDialog : public DPIDialog {
    Plater &m_plater;
    std::function<NativeAnalysisHostState()> m_capture;
    wxTextCtrl *m_editor, *m_report;
    wxStaticText *m_status, *m_movement;
    wxChoice *m_mode;
    wxButton *m_start, *m_cancel;
    wxSlider *m_slider;
    ReplayCanvas *m_canvas;
    wxTimer m_timer;
    uint64_t m_revision = 1;
    unsigned m_ticks = 0;
    bool m_close_pending = false;
    std::shared_ptr<AnalysisRun> m_run;
    std::shared_ptr<const NativeAnalysisViewInput> m_displayed;
    std::vector<std::array<double, 2>> m_times;
public:
    NativeAnalysisDialog(Plater &plater, std::function<NativeAnalysisHostState()> capture)
        : DPIDialog(&plater, wxID_ANY, "Nonplanar Top Lab — analysis", wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), m_plater(plater), m_capture(std::move(capture)), m_timer(this)
    {
        SetFont(wxGetApp().normal_font());
        auto *layout = new wxBoxSizer(wxVERTICAL);
        layout->Add(new wxStaticText(this, wxID_ANY, "Simulation analysis • export blocked • current plate/model/settings"), 0, wxALL, 8);
        auto *controls = new wxBoxSizer(wxHORIZONTAL);
        auto *load = new wxButton(this, wxID_ANY, "Load analysis request…"); controls->Add(load, 0, wxRIGHT, 8);
        m_mode = new wxChoice(this, wxID_ANY); m_mode->Append("safe_hybrid"); m_mode->Append("strict_nonplanar"); m_mode->SetSelection(0);
        controls->Add(m_mode, 0, wxRIGHT, 8);
        m_start = new wxButton(this, wxID_ANY, "Analyze"); controls->Add(m_start, 0, wxRIGHT, 8);
        m_cancel = new wxButton(this, wxID_ANY, "Cancel analysis"); m_cancel->Disable(); controls->Add(m_cancel);
        layout->Add(controls, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        m_editor = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, FromDIP(120)), wxTE_MULTILINE);
#ifdef __WXOSX__
        // JSON bytes must survive Cocoa's automatic quote/dash substitutions.
        m_editor->OSXDisableAllSmartSubstitutions();
#endif
        m_editor->SetFont(wxGetApp().code_font()); m_editor->SetMaxLength(2 * 1024 * 1024);
        m_editor->SetHint("Load the version 1 analysis JSON used by --nptop-analyze.");
        layout->Add(m_editor, 0, wxEXPAND | wxALL, 8);
        m_status = new wxStaticText(this, wxID_ANY, "Editing — no analysis result"); layout->Add(m_status, 0, wxLEFT | wxRIGHT, 8);
        auto *body = new wxBoxSizer(wxHORIZONTAL);
        m_canvas = new ReplayCanvas(this); m_canvas->SetMinSize(wxSize(FromDIP(350), FromDIP(180))); body->Add(m_canvas, 1, wxEXPAND | wxRIGHT, 8);
        m_report = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(350), -1), wxTE_MULTILINE | wxTE_READONLY);
#ifdef __WXOSX__
        m_report->OSXDisableAllSmartSubstitutions();
#endif
        m_report->SetFont(wxGetApp().code_font()); body->Add(m_report, 1, wxEXPAND);
        layout->Add(body, 1, wxEXPAND | wxALL, 8);
        auto *navigation = new wxBoxSizer(wxHORIZONTAL);
        auto *projection = new wxChoice(this, wxID_ANY); for (const char *p : {"XY", "XZ", "YZ"}) projection->Append(p); projection->SetSelection(1);
        navigation->Add(projection, 0, wxRIGHT, 8);
        m_slider = new wxSlider(this, wxID_ANY, 0, 0, 1); m_slider->Disable(); navigation->Add(m_slider, 1, wxEXPAND);
        layout->Add(navigation, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        m_movement = new wxStaticText(this, wxID_ANY, "Movement replay: unavailable"); layout->Add(m_movement, 0, wxEXPAND | wxALL, 8);
        auto *close = new wxButton(this, wxID_CLOSE, "Close"); layout->Add(close, 0, wxALIGN_RIGHT | wxALL, 8);
        SetSizer(layout); SetMinSize(wxSize(FromDIP(780), FromDIP(600))); SetSize(wxSize(FromDIP(1020), FromDIP(780)));
        wxGetApp().UpdateDarkUI(this); wxGetApp().UpdateDlgDarkUI(this); CenterOnParent();
        load->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            // Validate bytes with the shared grammar, rather than relying on
            // platform file-type associations in the isolated development app.
            wxFileDialog picker(this, "Analysis request (JSON)", wxEmptyString, wxEmptyString,
                wxFileSelectorDefaultWildcardStr, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (picker.ShowModal() != wxID_OK) return;
            edited();
            try {
                const auto file = capture_stl_file(picker.GetPath().ToUTF8().data(), true, m_revision);
                require(file.source && file.source->bytes.size() <= 2 * 1024 * 1024, "VIEW_REQUEST_FILE_REFUSED");
                (void)parse_native_analysis_document(file.source->bytes);
                m_editor->SetValue(wxString::FromUTF8(file.source->bytes));
            } catch (...) { m_status->SetLabel("Analysis request file refused — expected bounded version 1 JSON"); }
        });
        m_editor->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { edited(); });
        m_mode->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { edited(); });
        m_start->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { start(); });
        m_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { cancel(); });
        projection->Bind(wxEVT_CHOICE, [this, projection](wxCommandEvent &) { m_canvas->projection = projection->GetSelection(); m_canvas->Refresh(); });
        m_slider->Bind(wxEVT_SLIDER, [this](wxCommandEvent &) { movement(); });
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { close_requested(); });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) { if (m_run && e.CanVeto()) { e.Veto(); close_requested(); } else if (!m_run) EndModal(wxID_CLOSE); else { cancel(); e.Skip(); } });
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) { tick(); }); m_timer.Start(100);
    }
private:
    std::string bytes() const { return m_editor->GetValue().ToUTF8().data(); }
    std::shared_ptr<const NativeAnalysisViewInput> capture(const NativeAnalysisHostState &state) const
    {
        return capture_native_analysis_view_input(m_plater.model(), state.config, state.plate, state.origin,
            m_mode->GetStringSelection().ToUTF8().data(), bytes());
    }
    void clear_display()
    {
        m_displayed.reset(); m_canvas->moves = Json::array(); m_canvas->Refresh(); m_times.clear();
        m_slider->Disable(); m_report->Clear(); m_movement->SetLabel("Movement replay: unavailable");
    }
    void edited()
    {
        if (m_revision == std::numeric_limits<uint64_t>::max()) { m_start->Disable(); cancel(); return; }
        ++m_revision; if (m_run) m_run->cancelled.store(true, std::memory_order_relaxed);
        clear_display(); m_status->SetLabel("Editing — previous analysis invalidated");
    }
    void cancel()
    {
        if (m_run) { m_run->cancelled.store(true, std::memory_order_relaxed); m_status->SetLabel("Cancelling analysis…"); }
        clear_display();
    }
    void close_requested() { if (m_run) { m_close_pending = true; cancel(); } else EndModal(wxID_CLOSE); }
    void start()
    {
        if (m_run || !m_plater.get_ui_job_worker().is_idle()) return;
        if (m_revision == std::numeric_limits<uint64_t>::max()) { m_start->Disable(); return; }
        ++m_revision; // A fresh execution also gets a fresh dialog ticket.
        clear_display();
        try {
            auto state = m_capture(); const auto input = capture(state);
            m_run = std::make_shared<AnalysisRun>(m_plater.model(), std::move(state), m_revision, input);
            const auto executable = (boost::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToUTF8().data()).parent_path() /
                NPTOP_BUNDLED_ANALYSIS_WORKER).string();
            const auto run = m_run; wxWeakRef<NativeAnalysisDialog> self(this);
            const bool queued = queue_job(m_plater.get_ui_job_worker(),
                [run, executable](Job::Ctl &ctl) { run->process(ctl, executable); },
                [run, self](bool cancelled, std::exception_ptr &exception) {
                    if (exception) { run->result = {"VIEW_BACKGROUND_EXCEPTION", {}}; exception = nullptr; }
                    if (self) self->finish(run, cancelled);
                    else if (run->task) stop_guarded_job(*run->print, *run->task, GuardedJobPhase::Cancelled);
                });
            if (!queued) { m_run.reset(); m_status->SetLabel("Analysis queue refused"); return; }
            m_start->Disable(); m_cancel->Enable(); m_status->SetLabel("Analyzing — export blocked");
        } catch (const std::exception &) { m_run.reset(); m_status->SetLabel("Analysis inputs refused — check JSON, model and current plate"); }
        catch (...) { m_run.reset(); m_status->SetLabel("Analysis input exception"); }
    }
    void finish(const std::shared_ptr<AnalysisRun> &run, bool cancelled)
    {
        if (m_run != run) return;
        std::shared_ptr<const NativeAnalysisViewInput> current;
        try { current = capture(m_capture()); } catch (...) {}
        if (run->task) run->result = finish_native_analysis_view(*run->print, *run->task, *run->input, current.get(),
            run->revision, m_revision, cancelled || run->cancelled.load(std::memory_order_relaxed), std::move(run->result));
        else if (cancelled || run->cancelled.load(std::memory_order_relaxed)) run->result = {"VIEW_CANCELLED", {}};
        clear_display(); m_status->SetLabel(wxString::FromUTF8(run->result.reason) + " — export blocked");
        if (!run->result.diagnostic.empty()) {
            try {
                const auto diagnostic = Json::parse(run->result.diagnostic);
                m_canvas->moves = diagnostic.at("replay");
                Json report{{"reason", diagnostic.at("reason")}, {"export_allowed", false}, {"report", diagnostic.at("report")}};
                if (!diagnostic.at("job").is_null()) report["job"] = {{"id", diagnostic.at("job").at("id")},
                    {"revision", diagnostic.at("job").at("revision")}, {"fingerprint", diagnostic.at("job").at("fingerprint")}};
                report["host_revision"] = run->revision; report["host_input_sha256"] = run->input->identity_sha256;
                report["worker_peak_rss_bytes"] = run->result.peak_rss_bytes;
                m_report->ChangeValue(wxString::FromUTF8(report.dump(2)));
                m_displayed = current;
                if (!m_canvas->moves.empty()) {
                    std::array<double, 2> time{0, 0};
                    for (const auto &move : m_canvas->moves) {
                        time[0] = std::max(0., std::nextafter(time[0] + move.at("duration_s")[0].get<double>(), -std::numeric_limits<double>::infinity()));
                        time[1] = std::nextafter(time[1] + move.at("duration_s")[1].get<double>(), std::numeric_limits<double>::infinity());
                        require(std::isfinite(time[0]) && std::isfinite(time[1]), "VIEW_TIME_REFUSED");
                        m_times.push_back(time);
                    }
                    m_slider->SetRange(0, int(m_canvas->moves.size() - 1)); m_slider->SetValue(0); m_slider->Enable(); movement();
                }
            } catch (...) { clear_display(); m_status->SetLabel("Analysis display refused — export blocked"); }
        }
        m_run.reset(); m_start->Enable(); m_cancel->Disable();
        if (m_close_pending) EndModal(wxID_CLOSE);
    }
    void movement()
    {
        if (m_canvas->moves.empty()) return;
        m_canvas->selected = size_t(m_slider->GetValue()); const auto &move = m_canvas->moves.at(m_canvas->selected);
        static constexpr const char *kinds[]{"Deposit", "Travel", "Retraction", "Restore", "Dwell"};
        const auto kind = move.at("kind").get<unsigned>(); const auto time = m_times.at(m_canvas->selected);
        // The JSON round-trip representation can round a boundary by less
        // than one binary64 ulp. Expand before rendering the interval.
        const std::array<double, 2> displayed_time{
            std::max(0., std::nextafter(time[0], -std::numeric_limits<double>::infinity())),
            std::nextafter(time[1], std::numeric_limits<double>::infinity())};
        const auto text = "Movement " + std::to_string(m_canvas->selected) + "/" + std::to_string(m_canvas->moves.size() - 1) +
            "  " + (kind < 5 ? kinds[kind] : "Unknown") + "  t=" + Json(displayed_time).dump() + " s\n" +
            "XYZ " + move.at("start_mm").dump() + " → " + move.at("end_mm").dump() +
            "  E=" + move.at("e_mm").dump() + " mm  F=" + move.at("feed_mm_min").dump() + " mm/min\n" +
            "Nominal volume " + move.at("nominal_volume_mm3").dump() + " mm³  bytes " + move.at("candidate_byte_range").dump() +
            (m_canvas->moves.size() > 10000 ? "  (path preview LOD; selected movement exact)" : "");
        m_movement->SetLabel(wxString::FromUTF8(text)); m_canvas->Refresh(); Layout();
    }
    void tick()
    {
        // Modal dialogs cannot rely on the Plater's idle/paint delivery.
        m_plater.get_ui_job_worker().process_events();
        if (m_run && m_run->revision == m_revision && !m_run->cancelled.load(std::memory_order_relaxed)) {
            const auto stage = m_run->stage.load(std::memory_order_relaxed);
            if (stage >= 0) m_status->SetLabel(wxString("Analyzing: ") + native_analysis_stage_name(NativeAnalysisStage(stage)) + " — export blocked");
        }
        if (++m_ticks % 10 == 0 && (m_run || m_displayed)) {
            try {
                const auto current = capture(m_capture()); const auto previous = m_run ? m_run->input : m_displayed;
                if (current->identity_sha256 != previous->identity_sha256) edited();
            } catch (...) { edited(); }
        }
    }
    void on_dpi_changed(const wxRect &) override { Layout(); Refresh(); }
    void on_sys_color_changed() override { wxGetApp().UpdateDlgDarkUI(this); m_canvas->Refresh(); }
};
}
void show_native_analysis_dialog(Plater &plater, std::function<NativeAnalysisHostState()> capture)
{
    NativeAnalysisDialog dialog(plater, std::move(capture)); dialog.ShowModal();
}
}
