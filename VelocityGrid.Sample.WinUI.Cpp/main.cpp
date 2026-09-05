#include <windows.h>
#undef GetCurrentTime
#include <array>
#include <chrono>
#include <format>
#include <random>
#include <unordered_map>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/VelocityGrid_Native.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace VelocityGrid_Native;

struct SampleApp : ApplicationT<SampleApp>
{
    void Launch() { launch(); }

private:
    void launch()
    {
        m_window = Window();
        m_window.Title(L"VelocityGrid — C++/WinRT NuGet sample");

        Grid root;
        root.Padding(ThicknessHelper::FromUniformLength(16));
        root.RowDefinitions().Append(RowDefinition());
        root.RowDefinitions().Append(RowDefinition());
        root.RowDefinitions().GetAt(0).Height(GridLengthHelper::FromPixels(42));
        root.RowDefinitions().GetAt(1).Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

        StackPanel toolbar;
        toolbar.Orientation(Orientation::Horizontal);
        toolbar.Spacing(8);
        auto append = make_button(L"Append 1,000");
        auto invalidate = make_button(L"Invalidate rows 0–127");
        auto refresh = make_button(L"Refresh to top");
        auto jump_to_end = make_button(L"Jump near end");
        TextBlock status;
        status.Text(L"10,000,000 rows — NuGet.org package");
        status.Margin(ThicknessHelper::FromLengths(12, 0, 0, 0));
        status.VerticalAlignment(VerticalAlignment::Center);
        toolbar.Children().Append(append);
        toolbar.Children().Append(invalidate);
        toolbar.Children().Append(refresh);
        toolbar.Children().Append(jump_to_end);
        toolbar.Children().Append(status);
        root.Children().Append(toolbar);

        std::array<hstring, 6> headers{ L"Row", L"Symbol", L"Price", L"Status", L"Venue", L"Notes" };
        std::array<double, 6> widths{ 100, 150, 120, 130, 120, 260 };
        std::array<int32_t, 6> alignments{ 2, 0, 2, 0, 0, 0 };
        m_grid.SetColumns(headers, widths, alignments);
        m_grid.RowHeight(25);
        m_grid.RowCount(m_row_count);
        m_page_token = m_grid.PageRequested([this](int64_t start, int32_t count, uint64_t request, uint64_t generation)
        {
            complete_page(start, count, request, generation);
        });
        m_grid.ExternalProviderEnabled(true);

        m_market_timer.Interval(std::chrono::milliseconds(100));
        m_market_token = m_market_timer.Tick([this](auto&&, auto&&) { market_tick(); });
        m_market_timer.Start();

        auto const grid_element = m_grid.as<FrameworkElement>();
        Grid::SetRow(grid_element, 1);
        root.Children().Append(grid_element);
        append.Click([this, status](auto&&, auto&&)
        {
            m_row_count += 1'000;
            m_grid.NotifyDataChanged(m_row_count, DataChangeKind::Append);
            status.Text(std::format(L"{} rows", m_row_count));
        });
        invalidate.Click([this](auto&&, auto&&) { m_grid.InvalidateRows(0, 128); });
        refresh.Click([this](auto&&, auto&&) { m_grid.NotifyDataChangedWithOptions(m_row_count, DataChangeKind::Reset, true); });
        jump_to_end.Click([this, status](auto&&, auto&&)
        {
            auto const target = m_row_count > 100 ? m_row_count - 100 : 0;
            m_grid.ScrollToRow(target);
            status.Text(std::format(L"Near end — target row {} of {}", target, m_row_count));
        });

        m_window.Content(root);
        m_window.Activate();
    }

    static Button make_button(std::wstring_view text)
    {
        Button button;
        button.Content(box_value(text));
        return button;
    }

    void complete_page(int64_t start, int32_t count, uint64_t request, uint64_t generation)
    {
        constexpr int32_t columns = 6;
        com_array<hstring> values(static_cast<uint32_t>(count * columns));
        com_array<uint8_t> foregrounds(values.size());
        com_array<uint8_t> backgrounds(values.size());
        com_array<uint8_t> icons(values.size());
        for (int32_t row_offset = 0; row_offset < count; ++row_offset)
        {
            auto const row = start + row_offset;
            std::array<hstring, columns> cells{
                to_hstring(row), hstring{ std::format(L"SYM{:03}", row % 997) },
                hstring{ std::format(L"{:.2f}", 80.0 + (row % 4000) / 100.0) },
                row % 7 == 0 ? L"Updated" : L"Live", row % 2 == 0 ? L"LSE" : L"XNAS",
                hstring{ std::format(L"Native provider row {}", row) } };
            for (int32_t column = 0; column < columns; ++column)
                values[static_cast<uint32_t>(row_offset * columns + column)] = cells[column];
            auto const price = static_cast<uint32_t>(row_offset * columns + 2);
            foregrounds[price] = row % 2 == 0 ? 6 : 7; // Green or red from the native palette ABI.
            icons[price] = row % 2 == 0 ? 1 : 2;       // Up or down arrow.
        }
        m_grid.CompletePage(request, generation, start, count, values, foregrounds, backgrounds, icons);
    }

    void market_tick()
    {
        auto const now = std::chrono::steady_clock::now();
        for (auto iterator = m_flashes.begin(); iterator != m_flashes.end();)
        {
            if (iterator->second.due > now) { ++iterator; continue; }
            std::array<int64_t, 1> rows{ iterator->first };
            std::array<int32_t, 1> columns{ 2 };
            std::array<hstring, 1> values{ iterator->second.value };
            std::array<uint8_t, 1> foregrounds{ iterator->second.rising ? uint8_t{ 14 } : uint8_t{ 7 } };
            std::array<uint8_t, 1> backgrounds{};
            std::array<uint8_t, 1> icons{ iterator->second.rising ? uint8_t{ 1 } : uint8_t{ 2 } };
            m_grid.ApplyUpdates(rows, columns, values, foregrounds, backgrounds, icons);
            iterator = m_flashes.erase(iterator);
        }

        auto const first = m_grid.FirstVisibleRow();
        auto const last = m_grid.LastVisibleRow();
        if (last < first) return;
        std::uniform_int_distribution<int64_t> row_distribution(first, last);
        std::uniform_real_distribution<double> price_distribution(80.0, 120.0);
        std::bernoulli_distribution direction_distribution;
        std::array<int64_t, 3> rows{};
        std::array<int32_t, 3> columns{ 2, 2, 2 };
        std::array<hstring, 3> values{};
        std::array<uint8_t, 3> foregrounds{};
        std::array<uint8_t, 3> backgrounds{};
        std::array<uint8_t, 3> icons{};
        for (size_t index = 0; index < rows.size(); ++index)
        {
            auto const row = row_distribution(m_random);
            auto const rising = direction_distribution(m_random);
            auto const value = hstring{ std::format(L"{:.2f}", price_distribution(m_random)) };
            rows[index] = row;
            values[index] = value;
            foregrounds[index] = rising ? 14 : 7; // Green or red palette entry.
            backgrounds[index] = rising ? 15 : 8; // Light green or light red flash.
            icons[index] = rising ? 1 : 2;         // Up or down arrow.
            m_flashes[row] = { value, rising, now + std::chrono::milliseconds(500) };
        }
        m_grid.ApplyUpdates(rows, columns, values, foregrounds, backgrounds, icons);
    }

    struct pending_flash
    {
        hstring value;
        bool rising;
        std::chrono::steady_clock::time_point due;
    };

    Window m_window{ nullptr };
    VelocityGrid_Native::VelocityGrid m_grid;
    event_token m_page_token{};
    DispatcherTimer m_market_timer;
    event_token m_market_token{};
    std::mt19937 m_random{ 73 };
    std::unordered_map<int64_t, pending_flash> m_flashes;
    int64_t m_row_count{ 10'000'000 };
};

com_ptr<SampleApp> sample_app;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    init_apartment(apartment_type::single_threaded);
    Application::Start([](auto&&)
    {
        sample_app = make_self<SampleApp>();
        sample_app->Launch();
    });
    sample_app = nullptr;
    return 0;
}
