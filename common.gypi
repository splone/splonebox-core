{
  'variables': {
    'clang%': 0,
    'asan%': 0,
  },

  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG' ],
        'cflags': [
          '-fcolor-diagnostics',
          '-g',
          '-O0',
          '-fwrapv'
          ],
      },
      'Release': {
        'defines': ['NDEBUG'],
        'cflags': [
          '-O3',
          '-fstrict-aliasing',
        ],
      }
    },

  'conditions': [
    ['asan==1', {
      'cflags': [
        '-fsanitize=address',
        '-fno-omit-frame-pointer',
      ],
      'ldflags': [
        '-fsanitize=address',
      ]
    }]
   ]

  },

  'conditions': [
    ['clang==1', {
      'make_global_settings': [
        ['CC', '/usr/bin/clang'],
        ['CXX', '/usr/bin/clang++'],
        ['CC.host', '$(CC)'],
        ['CXX.host', '$(CXX)'],
      ],
    }],
  ],
}
