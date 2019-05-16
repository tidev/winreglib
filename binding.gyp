{
	'variables': {
	},
	'conditions': [
		['OS=="win"', {
			'targets': [
				{
					'target_name': 'node_winreglib',
					"include_dirs"  : [
						"<!(node -e \"require('napi-macros')\")"
					],
					'sources': [
						'src/watchman.cpp',
						'src/winreglib.cpp'
					],
					"msvs_settings": {
                        "VCCLCompilerTool": {
                            "RuntimeTypeInfo": "false",
                            "EnableFunctionLevelLinking": "true",
                            "ExceptionHandling": "2"
                        }
                    }
				}
			]
		}]
	]
}
