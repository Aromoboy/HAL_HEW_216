# HAL_HEW_216

## 必須アセット (Required Assets)
本リポジトリでは、以下のプリビルド済み DirectXTex ライブラリを常にバージョン管理下に置いています。

- `HEWGame/DirectXTex/Win32/Debug`
- `HEWGame/DirectXTex/Win32/Release`
- `HEWGame/DirectXTex/x64/Debug`
- `HEWGame/DirectXTex/x64/Release`

これらの `.lib` / `.dll` ファイルはリンカ設定で直接参照されており、リポジトリから削除するとビルドが失敗します。ツールチェーン（Visual Studio、Windows SDK、プラットフォーム ツールセットなど）を更新してライブラリを再生成する必要がある場合は、DirectXTex のソリューションを同じ構成でビルドし直し、上記ディレクトリに成果物を上書きしてください。その後、差分を確認したうえでコミットし、常に最新のバイナリを共有できるようにしてください。

また、`HEWGame/HEWGame.vcxproj.user` も意図的にバージョン管理されています。このファイルには Visual Studio 固有のユーザー設定が保存され、チーム全員のデバッガ設定やツールセット指定 (`ToolsVersion="Current"`) をそろえる役割を担っています。空の `<PropertyGroup>` を含めた最小構成ですが、Visual Studio が環境依存の値を勝手に書き込むのを防ぎ、設定差分を抑制する目的で維持しています。削除・無視すると開発環境ごとに挙動がばらつく原因になるため、リポジトリから除外しないでください。

## Core Update Scheduler

Core systems that need per-frame updates should inherit from `Core::IUpdatable`, register with the owning `Scene` (which forwards to its internal `Core::UpdateScheduler`), and toggle participation via the scheduler's enable/disable helpers as needed. Define `CORE_UPDATE_TRACE` to enable verbose trace output while diagnosing scheduler order or registration issues.
