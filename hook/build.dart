import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';

Future<void> _run(String executable, List<String> arguments) async {
  final result = await Process.run(executable, arguments);
  if (result.exitCode == 0) return;
  throw StateError(
    '$executable ${arguments.join(' ')} failed:\n'
    '${result.stdout}\n${result.stderr}',
  );
}

Future<void> main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    final code = input.config.code;
    if (code.targetOS == OS.android) {
      final abi = _androidAbi(code.targetArchitecture);
      if (abi == null) return;

      final library = File.fromUri(
        input.packageRoot.resolve(
          'native/android/$abi/libpcalc_clang_constexpr.so',
        ),
      );
      if (!library.existsSync()) return;

      output.assets.code.add(
        CodeAsset(
          package: input.packageName,
          name: 'pcalc_clang_constexpr',
          linkMode: DynamicLoadingBundled(),
          file: library.uri,
        ),
      );
      return;
    }
    if (code.targetOS != OS.linux) {
      return;
    }

    final buildDirectory = input.outputDirectoryShared.resolve('cmake/');
    final library = buildDirectory.resolve('libpcalc_clang_constexpr.so');
    await Directory.fromUri(buildDirectory).create(recursive: true);

    final configureArguments = <String>[
      '-S',
      input.packageRoot.toFilePath(),
      '-B',
      buildDirectory.toFilePath(),
      '-G',
      'Ninja',
      '-DCMAKE_BUILD_TYPE=Release',
      '-DPCALC_CONSTEXPR_BUILD_SHARED=ON',
    ];
    final llvmDir = Platform.environment['LLVM_DIR'];
    final clangDir = Platform.environment['Clang_DIR'];
    if (llvmDir != null) configureArguments.add('-DLLVM_DIR=$llvmDir');
    if (clangDir != null) configureArguments.add('-DClang_DIR=$clangDir');

    await _run('cmake', configureArguments);
    await _run('cmake', [
      '--build',
      buildDirectory.toFilePath(),
      '--target',
      'pcalc_clang_constexpr',
    ]);

    output.assets.code.add(
      CodeAsset(
        package: input.packageName,
        name: 'pcalc_clang_constexpr',
        linkMode: DynamicLoadingBundled(),
        file: library,
      ),
    );
  });
}

String? _androidAbi(Architecture architecture) {
  switch (architecture) {
    case Architecture.arm64:
      return 'arm64-v8a';
    default:
      return null;
  }
}
