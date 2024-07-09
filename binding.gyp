{
	'conditions': [
		['OS=="win"', {
			'targets': [
				{
					'target_name': 'node_winreglib',
					'include_dirs'  : [
						'<!(node -e "require(\'napi-macros\')")'
					],
					'defines': [
						"WINREGLIB_VERSION=\"<!(node -e \"console.log(require(\'./package.json\').version)\")\"",
						"WINREGLIB_URL=\"<!(node -e \"console.log(require(\'./package.json\').homepage)\")\""
					],
					'sources': [
						'src/watchnode.cpp',
						'src/watchman.cpp',
						'src/winreglib.cpp'
					],
					'msvs_settings': {
						'VCCLCompilerTool': {
							'RuntimeTypeInfo': 'false',
							'EnableFunctionLevelLinking': 'true',
							'ExceptionHandling': '2'
						}
					}
				}
			]
		}]
	]
}
