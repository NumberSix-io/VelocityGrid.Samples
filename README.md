# VelocityGrid NuGet samples

These three standalone applications are configured to consume VelocityGrid
`0.1.0-preview.7` exclusively from NuGet.org:

- `VelocityGrid.Sample.WinUI.CSharp` — C# WinUI 3.
- `VelocityGrid.Sample.WinUI.Cpp` — native C++/WinRT WinUI 3.
- `VelocityGrid.Sample.Wpf` — WPF with the packaged XAML Island host.

Each sample exposes 10,000,000 logical rows while VelocityGrid retains only its
bounded viewport cache. The samples also exercise live cell updates, append,
targeted invalidation, refresh, selection, and large-range scrolling.

## Prerequisites

- Windows 10 build 19041 or later.
- Visual Studio 2022 or later with Desktop development with C++, .NET desktop
  development, and Windows App SDK/C++/WinRT tooling.
- .NET 8 SDK.

## Run

Open `VelocityGrid.Samples.sln`
(`VelocityGrid.Samples.slnx` is also provided for newer Visual Studio versions),
select **Debug | x64**, choose one startup project, and run it. Both WinUI
executables use Microsoft-style self-contained Windows App SDK deployment. The
WPF package initializes its XAML Island host.

The package source and documentation are available from the
[VelocityGrid repository](https://github.com/NumberSix-io/VelocityGrid).
