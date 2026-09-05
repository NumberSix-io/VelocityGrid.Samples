using System.Windows;
using System.Threading;
using System.Threading.Tasks;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Threading;
using VelocityGrid.Managed;

namespace VelocityGrid.Sample.Wpf;

public partial class MainWindow : Window
{
    private readonly MutableSyntheticProvider _provider = new(10_000_000);
    private readonly DispatcherTimer _marketTimer = new() { Interval = TimeSpan.FromMilliseconds(100) };
    private readonly Random _random = new(73);
    private readonly Dictionary<long, PendingFlash> _flashes = new();

    public MainWindow()
    {
        InitializeComponent();
        GridHost.Columns = new[] { new VelocityGridColumn("row", "Row", 110), new VelocityGridColumn("symbol", "Symbol", 160), new VelocityGridColumn("price", "Price", 130, VelocityGridTextAlignment.Right), new VelocityGridColumn("status", "Status", 150), new VelocityGridColumn("notes", "Notes", 280) };
        GridHost.DataProvider = _provider;
        Status.Text = $"{_provider.RowCount:N0} rows — NuGet.org package";
        GridHost.GridReady += (_, _) => _marketTimer.Start();
        _marketTimer.Tick += MarketTick;
        Closed += (_, _) => _marketTimer.Stop();
    }

    private void Append_Click(object sender, RoutedEventArgs e) { _provider.Append(1_000); GridHost.NotifyDataChanged(_provider.RowCount, VelocityGridDataChangeKind.Append); Status.Text = $"{_provider.RowCount:N0} rows"; }
    private void Invalidate_Click(object sender, RoutedEventArgs e) => GridHost.InvalidateRows(0, 128);
    private void Refresh_Click(object sender, RoutedEventArgs e) => GridHost.Refresh(resetScrollPosition: true);
    private void End_Click(object sender, RoutedEventArgs e)
    {
        var target = Math.Max(0, _provider.RowCount - 100);
        GridHost.Grid?.ScrollToRow(target);
        Status.Text = $"Near end — target row {target:N0} of {_provider.RowCount:N0}";
    }

    private void MarketTick(object? sender, EventArgs e)
    {
        var grid = GridHost.Grid;
        if (grid is null) return;
        var now = DateTimeOffset.UtcNow;
        var clears = _flashes.Where(x => x.Value.Due <= now).ToArray();
        if (clears.Length > 0)
        {
            GridHost.ApplyUpdates(clears.Select(x => new VelocityGridCellUpdate(
                x.Key, 2, x.Value.Value, x.Value.RestingFormat)));
            foreach (var clear in clears)
                if (_flashes.TryGetValue(clear.Key, out var current) && current.Due == clear.Value.Due)
                    _flashes.Remove(clear.Key);
        }

        if (grid.LastVisibleRow < grid.FirstVisibleRow) return;
        var updates = new List<VelocityGridCellUpdate>();
        for (var i = 0; i < 3; i++)
        {
            var row = _random.NextInt64(grid.FirstVisibleRow, grid.LastVisibleRow + 1);
            var rising = _random.Next(2) == 0;
            var resting = new VelocityGridCellFormat(
                rising ? VelocityGridColor.Green : VelocityGridColor.Red,
                VelocityGridColor.None,
                rising ? VelocityGridIcon.UpArrow : VelocityGridIcon.DownArrow);
            var text = (80 + _random.NextDouble() * 40).ToString("F2");
            updates.Add(new(row, 2, text, resting with
            {
                Background = rising ? VelocityGridColor.LightGreen : VelocityGridColor.LightRed
            }));
            _flashes[row] = new(text, resting, now.AddMilliseconds(500));
        }
        GridHost.ApplyUpdates(updates);
    }

    private readonly record struct PendingFlash(string Value, VelocityGridCellFormat RestingFormat, DateTimeOffset Due);
}

internal sealed class MutableSyntheticProvider(long rowCount) : IVelocityGridDataProvider
{
    private readonly SyntheticDataProvider _inner = new(long.MaxValue);
    public long RowCount { get; private set; } = rowCount;
    public void Append(long count) => RowCount += count;
    public ValueTask<VelocityGridPage> GetRowsAsync(VelocityGridRange range, VelocityGridFetchContext context,
        CancellationToken cancellationToken) => _inner.GetRowsAsync(range, context, cancellationToken);
}
