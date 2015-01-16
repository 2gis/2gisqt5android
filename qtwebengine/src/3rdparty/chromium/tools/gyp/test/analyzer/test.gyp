# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'test_variable%': 0,
    'variable_path': 'subdir',
   },
  'targets': [
    {
      'target_name': 'exe',
      'type': 'executable',
      'dependencies': [
        'subdir/subdir.gyp:foo',
      ],
      'sources': [
        'foo.c',
        '<(variable_path)/subdir_source2.c',
      ],
      'conditions': [
        ['test_variable==1', {
          'sources': [
            'conditional_source.c',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'action',
          'inputs': [
            '<(PRODUCT_DIR)/product_dir_input.c',
            'action_input.c',
          ],
          'outputs': [
            'action_output.c',
          ],
        },
      ],
      'rules': [
        {
          'rule_name': 'rule',
          'extension': 'pdf',
          'inputs': [
            'rule_input.c',
          ],
          'outputs': [
            'rule_output.pdf',
          ],
        },
      ],
    },
  ],
}
