<?php

/**
 * @file
 * Admin forms for viewing/adding/editing/deleting tests, conditions, and experiences
 */


/**
* Sets variables that will be used in the testing script that is output (see abjs_preprocess_html)
*/
function abjs_settings_admin($form, &$form_state) {
  //Each applicable test will have one cookie. The cookie prefix will prefix the name of all test cookies.
  $form['abjs_cookie_prefix'] = array(
    '#type' => 'textfield', 
    '#title' => t('Cookie Prefix'), 
    '#default_value' => variable_get('abjs_cookie_prefix'), 
    '#description' => 'This string will prefix all A/B test cookie names',
    '#size' => 10, 
    '#maxlength' => 10, 
  );
  $form['abjs_cookie_lifetime'] = array(
    '#type' => 'textfield', 
    '#title' => t('Cookie Lifetime'), 
    '#description' => 'Enter cookie lifetime in days',
    '#default_value' => variable_get('abjs_cookie_lifetime'), 
    '#size' => 4, 
    '#maxlength' => 10, 
  );
  $form['abjs_cookie_domain'] = array(
    '#type' => 'textfield', 
    '#title' => t('Cookie Domain'), 
    '#description' => 'Leave blank to use current domain',
    '#default_value' => variable_get('abjs_cookie_domain'), 
    '#size' => 50, 
    '#maxlength' => 100, 
  );
  $form['abjs_cookie_secure'] = array(
    '#type' => 'select', 
    '#title' => t('Use Secure Cookies?'), 
    '#description' => 'This sets the secure flag on A/B test cookies',
    '#options' => array(0 => t('No'), 1 => t('Yes'), ),
    '#default_value' => variable_get('abjs_cookie_secure'), 
  );
  $form['abjs_ace'] = array(
    '#type' => 'select', 
    '#title' => t('Use Ace Code Editor?'), 
    '#description' => 'Use Ace Code Editor for entering Condition and Experience scripts',
    '#options' => array(0 => t('No'), 1 => t('Yes'), ),
    '#default_value' => variable_get('abjs_ace'), 
  );
  return system_settings_form($form);
}




//-------------Tests functions-----------------//
/**
* Lists all tests in a default table theme, sorted by active tests first, then modified most recently, then created most recently.
* For each test, link to test, and list active status, conditions applied (with links), experiences with fractions assigned (with links), and created and edited info
*/
function abjs_test_admin() {
  $header = array('ID', 'Name', 'Status', 'Conditions', 'Experiences', 'Created', 'Created By', 'Changed', 'Changed By');
  $rows = array();
  $active_array = array('Inactive', 'Active');
  $tests = db_query("SELECT * FROM {abjs_test} ORDER BY active DESC, changed DESC, created DESC");
  foreach ($tests as $test) {
    $condition_list = "";
    $conditions = db_query("SELECT tc.cid, c.name FROM {abjs_test_condition} AS tc INNER JOIN {abjs_condition} AS c ON tc.cid = c.cid WHERE tc.tid = :tid", array(':tid' => $test->tid));
    if (!empty($conditions)) {
      $condition_list .= "<ul>";
      foreach ($conditions as $condition) {
        $condition_list .= '<li>' . l($condition->name . ' (' . $condition->cid . ')', '/admin/config/abjs/conditions/' . $condition->cid . '/edit/') . '</li>';
      }
      $condition_list .= "</ul>";      
    }
    $experience_list = "";
    $experiences = db_query("SELECT te.eid, te.fraction, e.name FROM {abjs_test_experience} AS te INNER JOIN abjs_experience AS e ON te.eid = e.eid WHERE te.tid = :tid", array(':tid' => $test->tid));
    if (!empty($experiences)) {
      $experience_list .= "<ul>";
      foreach ($experiences as $experience) {
        $experience_list .= '<li>' . $experience->fraction . ' ' . l($experience->name . ' (' . $experience->eid . ')', '/admin/config/abjs/experiences/' . $experience->eid . '/edit/') . '</li>';
      }
      $experience_list .= "</ul>";      
    }    
    $user_created=user_load($test->created_by);
    $user_changed=user_load($test->changed_by);
    $rows[] = array($test->tid,
                    l($test->name, '/admin/config/abjs/tests/' . $test->tid . '/edit/'),
                    $active_array[$test->active],
                    $condition_list,
                    $experience_list,
                    format_date($test->created),
                    $user_created->name,
                    format_date($test->changed),
                    $user_changed->name,
                );
  }
  return theme('table', array('header' => $header, 'rows' => $rows)) . '<br>' . l(t('Add new test'), '/admin/config/abjs/tests/add/');
}


/**
* Generates a form for adding and editing tests. $tid will be NULL when adding a test,
* and will be a number when editing tests. $tid is checked throughout the function for determining if
* this is an add form or edit form.
*/
function abjs_test_form($form, &$form_state, $tid = NULL) {
  $form = array();
  $test_name_default = "";
  $test_active_default = 0;
  if (!empty($tid)) {
    //Retrieve the test to prefill the edit form
    $test_result = db_query('SELECT name, active FROM {abjs_test} WHERE tid = :tid', array(':tid' => $tid));
    if (empty($test_result)) {
      drupal_set_message(t('The requested test does not exist.'), 'error');
      return $form;
    }
    $test = $test_result->fetchObject();
    $test_name_default = $test->name;
    $test_active_default = $test->active;
    $form['tid'] = array('#type' => 'value', '#value' => $tid);
  }
  // Because we have many fields with the same values, we have to set
  // #tree to be able to access them.
  $form['#tree'] = TRUE;
  $form['name'] = array(
    '#type' => 'textfield', 
    '#title' => t('Test Name'), 
    '#default_value' => $test_name_default, 
    '#size' => 30, 
    '#maxlength' => 50,
    '#required' => TRUE, 
  );

  //Make select list of conditions
  $conditions = db_query("SELECT cid, name FROM {abjs_condition} ORDER BY cid ASC, created DESC");
  $options_array = array(0 => 'Select Condition');
  foreach ($conditions as $condition) {
    $options_array[$condition->cid] = $condition->name . ' (' . $condition->cid . ')';
  }

  //Group conditions together, and allow for adding and removing conditions via AJAX incide this fieldset
  $form['conditions_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Conditions'),
    // Set up the wrapper so that AJAX will be able to replace the fieldset.
    '#prefix' => '<div id="conditions-fieldset-wrapper">',
    '#suffix' => '</div>',
    '#description' => t('Select Conditions for which the test will apply.'),
  );

  $existing_conditions_count = 0;
  if (empty($form_state['num_conditions'])) {
    //On initial load, get the number of condition select fields (1 for add form, query the number for edit form)
    $form_state['num_conditions'] = 1;
    if (!empty($tid)) {
      $existing_conditions = db_query("SELECT cid FROM {abjs_test_condition} WHERE tid = :tid", array(':tid' => $tid));
      if (!empty($existing_conditions)) {
        $existing_conditions_count =  $existing_conditions->rowCount();
        $form_state['num_conditions'] = $existing_conditions_count;
      }    
    }
  }

  //Prefill all the condition select fields that exist
  for ($i = 0; $i < $existing_conditions_count; $i++) {
    $existing_condition = $existing_conditions->fetchObject();
    $form['conditions_fieldset']['conditions'][$i] = array(
      '#type' => 'select', 
      '#title' => t('Select Condition'), 
      '#options' => $options_array,
      '#default_value' => $existing_condition->cid,
      '#required' => TRUE,
    );
  }

  //Add number of sesgment fields determined by use of Ajax Add and remove buttons
  for ($i = $existing_conditions_count; $i < $form_state['num_conditions']; $i++) {
    $form['conditions_fieldset']['conditions'][$i] = array(
      '#type' => 'select', 
      '#title' => t('Select Condition'), 
      '#options' => $options_array,
      '#default_value' => 0,
      '#required' => TRUE,
    );
  }

  //Ajax add button
  $form['conditions_fieldset']['add_condition'] = array(
    '#type' => 'submit',
    '#value' => t('Add another condition'),
    '#submit' => array('ajax_add_condition'),
    '#ajax' => array(
      'callback' => 'ajax_conditions_callback',
      'wrapper' => 'conditions-fieldset-wrapper',
    ),
    '#limit_validation_errors' => array(),
  );

  //Ajax Remove button
  if ($form_state['num_conditions'] > 1) {
    $form['conditions_fieldset']['remove_condition'] = array(
      '#type' => 'submit',
      '#value' => t('Remove last condition'),
      '#submit' => array('ajax_remove_condition'),
      '#ajax' => array(
        'callback' => 'ajax_conditions_callback',
        'wrapper' => 'conditions-fieldset-wrapper',
      ),
      '#limit_validation_errors' => array(),
    );
  }

  //Now do the same for experiences

  //Make select list of experiences
  $experiences = db_query("SELECT eid, name FROM {abjs_experience} ORDER BY changed DESC, created DESC");
  $options_array = array(0 => 'Select Experience');
  foreach ($experiences as $experience) {
    $options_array[$experience->eid] = $experience->name . ' (' . $experience->eid . ')';
  }

  //Group experiences together, and allow for adding and removing experiences via AJAX incide this fieldset
  $form['experiences_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Experiences'),
    // Set up the wrapper so that AJAX will be able to replace the fieldset.
    '#prefix' => '<div id="experiences-fieldset-wrapper">',
    '#suffix' => '</div>',
    '#description' => t('Select one or more Experiences for the test, and assign fractions to each Experience (e.g. 1/2, 1/3, 0, 1, 0.5, .95, etc...)'),
  );
  $existing_experiences_count = 0;
  if (empty($form_state['num_experiences'])) {
    //On initial load, get the number of experience select fields (1 for add form, query the number for edit form)
    $form_state['num_experiences'] = 1;
    if (!empty($tid)) {
      $existing_experiences = db_query("SELECT eid, fraction FROM {abjs_test_experience} WHERE tid = :tid", array(':tid' => $tid));
      if (!empty($existing_experiences)) {
        $existing_experiences_count =  $existing_experiences->rowCount();
        $form_state['num_experiences'] = $existing_experiences_count;
      }
    }
  }

  //Prefill all the experience select fields that exist, including fractions for each experience
  for ($i = 0; $i < $existing_experiences_count; $i++) {
    $existing_experience = $existing_experiences->fetchObject();
    $form['experiences_fieldset']['experiences'][$i] = array(
      '#type' => 'select', 
      '#title' => t('Experience %i', array('%i' => $i)), 
      '#options' => $options_array,
      '#default_value' => $existing_experience->eid,
      '#required' => TRUE,
    );
    $form['experiences_fieldset']['experience_fractions'][$i] = array(
      '#type' => 'textfield', 
      '#title' => t('Experience %i Fraction', array('%i' => $i)), 
      '#default_value' => $existing_experience->fraction, 
      '#size' => 5, 
      '#maxlength' => 10, 
      '#required' => TRUE,
    );
  }

  //Add number of experience fields determined by use of Ajax Add and remove buttons
  for ($i = $existing_experiences_count; $i < $form_state['num_experiences']; $i++) {
    $form['experiences_fieldset']['experiences'][$i] = array(
      '#type' => 'select', 
      '#title' => t('Experience %i', array('%i' => $i)), 
      '#options' => $options_array,
      '#default_value' => 0,
      '#required' => TRUE,
    );
    $form['experiences_fieldset']['experience_fractions'][$i] = array(
      '#type' => 'textfield', 
      '#title' => t('Experience %i Fraction', array('%i' => $i)), 
      '#default_value' => '', 
      '#size' => 5, 
      '#maxlength' => 10,
      '#required' => TRUE, 
    );
  }

  //Ajax add button
  $form['experiences_fieldset']['add_experience'] = array(
    '#type' => 'submit',
    '#value' => t('Add another experience'),
    '#submit' => array('ajax_add_experience'),
    '#ajax' => array(
      'callback' => 'ajax_experiences_callback',
      'wrapper' => 'experiences-fieldset-wrapper',
    ),
    '#limit_validation_errors' => array(),
  );

  //Ajax Remove button
  if ($form_state['num_experiences'] > 1) {
    $form['experiences_fieldset']['remove_experience'] = array(
      '#type' => 'submit',
      '#value' => t('Remove last experience'),
      '#submit' => array('ajax_remove_experience'),
      '#ajax' => array(
        'callback' => 'ajax_experiences_callback',
        'wrapper' => 'experiences-fieldset-wrapper',
      ),
      '#limit_validation_errors' => array(),
    );
  }

  //Add selector for activating/deactivating test
  $form['active'] = array(
    '#type' => 'select', 
    '#title' => t('Status'), 
    '#options' => array(
      0 => t('Inactive'),
      1 => t('Active'),
     ),
    '#default_value' => $test_active_default,
  );

  //Save test
  $form['actions']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 5,
    '#submit' => array('abjs_test_form_save_submit'),
  );

  //Cancel test and return to admin tests page
  $form['actions']['cancel'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#weight' => 10,
    '#submit' => array('abjs_test_form_cancel_submit'),
    '#limit_validation_errors' => array(),
  );

  //Delete test
  if (!empty($tid)) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#weight' => 15,
      '#submit' => array('abjs_test_form_delete_submit'),
    );
  }
  return $form;
}

/**
* Adds condition select fields to abjs_test_form 
*/
function ajax_add_condition($form, &$form_state) {
  $form_state['num_conditions']++;
  $form_state['rebuild'] = TRUE;
}

/**
* Assigns Ajax changes to conditions fieldset in abjs_test_form 
*/
function ajax_conditions_callback($form, $form_state) {
  return $form['conditions_fieldset'];
}

/**
* Removes condition select fields to abjs_test_form 
*/
function ajax_remove_condition($form, &$form_state) {
  if ($form_state['num_conditions'] > 1) {
    $form_state['num_conditions']--;
  }
  $form_state['rebuild'] = TRUE;
}

/**
* Adds experience select fields and fractions to abjs_test_form 
*/
function ajax_add_experience($form, &$form_state) {
  $form_state['num_experiences']++;
  $form_state['rebuild'] = TRUE;
}

/**
* Assigns Ajax changes to experiences fieldset in abjs_test_form 
*/
function ajax_experiences_callback($form, $form_state) {
  return $form['experiences_fieldset'];
}

/**
* Removes experience select fields and fractions to abjs_test_form 
*/
function ajax_remove_experience($form, &$form_state) {
  if ($form_state['num_experiences'] > 1) {
    $form_state['num_experiences']--;
  }
  $form_state['rebuild'] = TRUE;
}



/**
* Save function for new and edited tests. Check for new vs edit by using tid
*/
function abjs_test_form_save_submit($form, &$form_state) {
  global $user; 
  if (!empty($form_state['values']['tid'])) {
    //This is an existing test, so update instead of insert
    $tid = $form_state['values']['tid'];
    db_update('abjs_test')
      ->fields(array(
        'name' => $form_state['values']['name'],
        'active' => $form_state['values']['active'],
        'changed' => REQUEST_TIME,
        'changed_by' => $user->uid,        
    ))->condition('tid', $tid, '=')
      ->execute();

    //Delete all entries in the test-condition and test-experience tables to make
    //life easy. Re-insert them later in this function.
    db_delete('abjs_test_condition')
        ->condition('tid', $tid)
        ->execute();
    db_delete('abjs_test_experience')
        ->condition('tid', $tid)
        ->execute();   
  }
  else{
    //This is a new test, so insert it
    $tid = db_insert('abjs_test')
      ->fields(array(
        'name' => $form_state['values']['name'],
        'active' => $form_state['values']['active'],
        'created' => REQUEST_TIME,
        'created_by' => $user->uid,
        'changed' => REQUEST_TIME,
        'changed_by' => $user->uid,        
      ))->execute();    
  }

  //Whether new or existing test, insert conditions and experiences for this test
  //into the test-condition and test-experience tables. If this is an existing test,
  //an earlier step deleted all the existing rows for this test in these tables.
  //Using db_merge instead of db_insert in case multiple of the same condition
  //or experience were entered, in which case this will collapse them to one.
  foreach ($form_state['values']['conditions_fieldset']['conditions'] as $cid) {
    if ($cid > 0) {
      db_merge('abjs_test_condition')
        ->key(array(
          'tid' => $tid,
          'cid' => $cid,       
      ))->fields(array(
          'tid' => $tid,
          'cid' => $cid,       
      ))->execute();      
    }
  }
  for ($i = 0; $i < count($form_state['values']['experiences_fieldset']['experiences']); $i++) {
    if ($form_state['values']['experiences_fieldset']['experiences'][$i] > 0) {
      db_merge('abjs_test_experience')
        ->key(array(
          'tid' => $tid,
          'eid' => $form_state['values']['experiences_fieldset']['experiences'][$i],        
      ))->fields(array(
          'tid' => $tid,
          'eid' => $form_state['values']['experiences_fieldset']['experiences'][$i],
          'fraction' => $form_state['values']['experiences_fieldset']['experience_fractions'][$i],        
      ))->execute();      
    }
  }
  $msg = !empty($form_state['values']['tid']) ? t("Successfully updated test") : t("Successfully saved new test");
  drupal_set_message(check_plain($msg)); 
  drupal_goto('/admin/config/abjs/tests');
}

/**
* Redirects back to tests admin page
*/
function abjs_test_form_cancel_submit($form, &$form_state) {
  $form_state['redirect'] = array('admin/config/abjs/tests/');
}

/**
* Redirects to delete path
*/
function abjs_test_form_delete_submit($form, &$form_state) {
  $form_state['redirect'] = array('admin/config/abjs/tests/' . $form_state['values']['tid'] . '/delete');
}

/**
* Returns confirmation form for deleting a test
*/
function abjs_test_delete_confirm_form($form, &$form_state, $tid) {
  $form = array();
  $test_result = db_query('SELECT name FROM {abjs_test} WHERE tid = :tid', array(':tid' => $tid));
  if (!empty($test_result)) {
    $form['tid'] = array('#type' => 'value', '#value' => $tid);
    $test = $test_result->fetchObject();
    $form['name'] = array('#type' => 'value', '#value' => $test->name);
    return confirm_form($form,
      t('Are you sure you want to delete this test: %name?', array('%name' => $test->name)),
      'admin/config/abjs/tests',
      t('This action cannot be undone.'),
      t('Delete'),
      t('Cancel')
    );
  }
  return $form;
}

/**
* Deletes test and associated test-condition and test-experience rows 
*/
function abjs_test_delete_confirm_form_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    db_delete('abjs_test')
      ->condition('tid', $form_state['values']['tid'])
      ->execute();
    db_delete('abjs_test_condition')
      ->condition('tid', $form_state['values']['tid'])
      ->execute();
    db_delete('abjs_test_experience')
      ->condition('tid', $form_state['values']['tid'])
      ->execute();

    drupal_set_message(t('Test "%name" has been deleted.', array('%name' => $form_state['values']['name'])));
  }

  $form_state['redirect'] = 'admin/config/abjs/tests';
}


//-------------End Tests functions-----------------//


//-------------Condition functions-----------------//



/**
* Lists all conditions in default table, sorted by modified date
* For each condition, link to edit form, and list created and edited info
*/
function abjs_condition_admin() {
  $header = array('ID', 'Name', 'Created', 'Created By', 'Changed', 'Changed By');
  $rows = array();

  $conditions = db_query("SELECT * FROM {abjs_condition} ORDER BY changed DESC, created DESC");
  foreach ($conditions as $condition) {
    $user_created=user_load($condition->created_by);
    $user_changed=user_load($condition->changed_by);
    $rows[] = array($condition->cid,
                    l($condition->name, '/admin/config/abjs/conditions/' . $condition->cid . '/edit/'),
                    format_date($condition->created),
                    $user_created->name,
                    format_date($condition->changed),
                    $user_changed->name,
                );
  }
  return theme('table', array('header' => $header, 'rows' => $rows)) . '<br>' . l(t('Add new condition'), '/admin/config/abjs/conditions/add/');
}

/**
* Generates form for creating and editing conditions, each consisting of a name and javascript field. $cid will be NULL when adding a condition,
* and will be a number when editing conditions. $cid is checked throughout the function for determining if
* this is an add form or edit form.
*/
function abjs_condition_form($form, &$form_state, $cid = NULL) {
  $form = array();
  $condition_name_default = "";
  $condition_script_default = "";
  if (!empty($cid)) {
    $condition_result = db_query('SELECT name, script FROM {abjs_condition} WHERE cid = :cid', array(':cid' => $cid));
    if (empty($condition_result)) {
      drupal_set_message(t('The requested condition does not exist.'), 'error');
      return $form;
    }
    $condition = $condition_result->fetchObject();    
    $condition_name_default = $condition->name;
    $condition_script_default = $condition->script;
    $form['cid'] = array('#type' => 'value', '#value' => $cid);
  }
  $form['name'] = array(
    '#type' => 'textfield', 
    '#title' => t('Condition Name'), 
    '#default_value' => $condition_name_default, 
    '#size' => 30, 
    '#maxlength' => 50, 
    '#required' => TRUE,
  );

  $form['script'] = array(
    '#type' => 'textarea', 
    '#title' => t('Condition Script'), 
    '#default_value' => $condition_script_default,
    '#attributes' => array('placeholder' => array("EXAMPLE: return window.location.pathname == '/'")),
    '#description' => t('Any valid javascript with a return statement at the end, returning true or false. Read the documentation for examples'),
    '#rows' => 3, 
    '#required' => TRUE,
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 5,
    '#submit' => array('abjs_condition_form_save_submit'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#weight' => 10,
    '#submit' => array('abjs_condition_form_cancel_submit'),
    '#limit_validation_errors' => array(),
  );
  if (!empty($cid)) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#weight' => 15,
      '#submit' => array('abjs_condition_form_delete_submit'),
    );
  }

  //Add ace code editor for syntax highlighting on the script field
  if (variable_get('abjs_ace') == 1) {
    drupal_add_js('https://cdnjs.cloudflare.com/ajax/libs/ace/1.2.0/ace.js', array('type' => 'external', 'scope' => 'header', 'group' => JS_LIBRARY));
    drupal_add_js(drupal_get_path('module', 'abjs') . '/abjs-ace-edit.js');
  }

  return $form;
}

/**
* Saves new and modified conditions into condition table
*/
function abjs_condition_form_save_submit($form, &$form_state) {
  global $user;
  if (!empty($form_state['values']['cid'])) {
    //This is a modified condition, so use update
    db_update('abjs_condition')
      ->fields(array(
        'name' => $form_state['values']['name'],
        'script' => $form_state['values']['script'],
        'changed' => REQUEST_TIME,
        'changed_by' => $user->uid,        
      ))
      ->condition('cid', $form_state['values']['cid'], '=')
      ->execute();
    drupal_set_message(t("Successfully updated condition")); 

  }
  else{
    //This is a new condition, so use insert
    db_insert('abjs_condition')
      ->fields(array(
        'name' => $form_state['values']['name'],
        'script' => $form_state['values']['script'],
        'created' => REQUEST_TIME,
        'created_by' => $user->uid,
        'changed' => REQUEST_TIME,
        'changed_by' => $user->uid,        
      ))->execute();
    drupal_set_message(t("Successfully saved new condition")); 

  }
  drupal_goto('/admin/config/abjs/conditions');
}

/**
* Redirects to condition admin page
*/
function abjs_condition_form_cancel_submit($form, &$form_state) {
  $form_state['redirect'] = array('admin/config/abjs/conditions/');
}

/**
* Redirects to condition delete path
*/
function abjs_condition_form_delete_submit($form, &$form_state) {
  $form_state['redirect'] = array('admin/config/abjs/conditions/' . $form_state['values']['cid'] . '/delete');
}

/**
* Returns confirm form for deleting condition
*/
function abjs_condition_delete_confirm_form($form, &$form_state, $cid) {
  $form = array();
  $condition_result = db_query('SELECT name FROM {abjs_condition} WHERE cid = :cid', array(':cid' => $cid));
  if (!empty($condition_result)) {
    $form['cid'] = array('#type' => 'value', '#value' => $cid);
    $condition = $condition_result->fetchObject();
    $form['name'] = array('#type' => 'value', '#value' => $condition->name);
    return confirm_form($form,
      t('Are you sure you want to delete this condition: %name?', array('%name' => $condition->name)),
      'admin/config/abjs/conditions',
      t('This action cannot be undone.'),
      t('Delete'),
      t('Cancel')
    );
  }
  return $form;
}

/**
* Deletes condition from condition table, as well as all test-condition entries for this condition
*/
function abjs_condition_delete_confirm_form_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    db_delete('abjs_condition')
      ->condition('cid', $form_state['values']['cid'])
      ->execute();
    db_delete('abjs_test_condition')
      ->condition('cid', $form_state['values']['cid'])
      ->execute();

    drupal_set_message(t('Condition "%name" has been deleted.', array('%name' => $form_state['values']['name'])));
  }

  $form_state['redirect'] = 'admin/config/abjs/conditions';
}



//-------------End Condition functions-----------------//


//-------------Experience functions-----------------//



/**
* Lists all experiences in default table, sorted by modified date
* For each experience, link to edit form, and list created and edited info
*/
function abjs_experience_admin() {
  $header = array('ID', 'Name', 'Created', 'Created By', 'Changed', 'Changed By');
  $rows = array();

  $experiences = db_query("SELECT * FROM {abjs_experience} ORDER BY changed DESC, created DESC");
  foreach ($experiences as $experience) {
    $user_created=user_load($experience->created_by);
    $user_changed=user_load($experience->changed_by);
    $rows[] = array($experience->eid,
                    l($experience->name, '/admin/config/abjs/experiences/' . $experience->eid . '/edit/'),
                    format_date($experience->created),
                    $user_created->name,
                    format_date($experience->changed),
                    $user_changed->name,
                );
  }
  return theme('table', array('header' => $header, 'rows' => $rows)) . '<br>' . l(t('Add new experience'), '/admin/config/abjs/experiences/add/');
}


/**
* Generates form for creating and editing experiences, each consisting of a name and javascript field. $eid will be NULL when adding an experience,
* and will be a number when editing experiences. $eid is checked throughout the function for determining if
* this is an add form or edit form.
*/
function abjs_experience_form($form, &$form_state, $eid = NULL) {
  $form = array();
  $experience_name_default = "";
  $experience_script_default = "";
  if (!empty($eid)) {
    $experience_result = db_query('SELECT name, script FROM {abjs_experience} WHERE eid = :eid', array(':eid' => $eid));
    if (empty($experience_result)) {
      drupal_set_message(t('The requested experience does not exist.'), 'error');
      return $form;
    }
    $experience = $experience_result->fetchObject();    
    $experience_name_default = $experience->name;
    $experience_script_default = $experience->script;
    $form['eid'] = array('#type' => 'value', '#value' => $eid);
  }

  $form['name'] = array(
    '#type' => 'textfield', 
    '#title' => t('Experience Name'), 
    '#default_value' => $experience->name, 
    '#size' => 30, 
    '#maxlength' => 50, 
    '#required' => TRUE,
  );

  $form['script'] = array(
    '#type' => 'textarea', 
    '#title' => t('Experience Script'), 
    '#default_value' => $experience->script,
    '#attributes' => array('placeholder' => array('EXAMPLE: document.write("<style>.sign-up-button { display:none !important; }</style>");')),
    '#description' => t('Any valid javascript to load in head. Leave empty for a Control. Read the documentation for more examples.'),
    '#rows' => 3,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 5,
    '#submit' => array('abjs_experience_form_save_submit'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#weight' => 10,
    '#submit' => array('abjs_experience_form_cancel_submit'),
    '#limit_validation_errors' => array(),
  );
  if (!empty($eid)) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#weight' => 15,
      '#submit' => array('abjs_experience_form_delete_submit'),
    );    
  }

  //Add ace code editor for syntax highlighting on the script field
  if (variable_get('abjs_ace') == 1) {
    drupal_add_js('https://cdnjs.cloudflare.com/ajax/libs/ace/1.2.0/ace.js', array('type' => 'external', 'scope' => 'header', 'group' => JS_LIBRARY));
    drupal_add_js(drupal_get_path('module', 'abjs') . '/abjs-ace-edit.js', array('type' => 'file', 'scope' => 'footer', 'group' => JS_THEME));
  }
  return $form;
}

/**
* Saves new and modified experiences into experience table
*/
function abjs_experience_form_save_submit($form, &$form_state) {
  global $user; 
  if (!empty($form_state['values']['eid'])) {
    //This is a modified experience, so use update
    db_update('abjs_experience')
      ->fields(array(
        'name' => $form_state['values']['name'],
        'script' => $form_state['values']['script'],
        'changed' => REQUEST_TIME,
        'changed_by' => $user->uid,        
      ))
      ->condition('eid', $form_state['values']['eid'], '=')
      ->execute();
    drupal_set_message(t("Successfully updated experience")); 
  }
  else{
    //This is a new experience, so use insert
    db_insert('abjs_experience')
      ->fields(array(
        'name' => $form_state['values']['name'],
        'script' => $form_state['values']['script'],
        'created' => REQUEST_TIME,
        'created_by' => $user->uid,
        'changed' => REQUEST_TIME,
        'changed_by' => $user->uid,        
      ))->execute();
    drupal_set_message(t("Successfully saved new experience")); 
  }
  drupal_goto('/admin/config/abjs/experiences');
}

/**
* Redirects to experience admin page
*/
function abjs_experience_form_cancel_submit($form, &$form_state) {
  $form_state['redirect'] = array('admin/config/abjs/experiences/');
}

/**
* Redirects to experience delete path for this experience
*/
function abjs_experience_form_delete_submit($form, &$form_state) {
  $form_state['redirect'] = array('admin/config/abjs/experiences/' . $form_state['values']['eid'] . '/delete');
}

/**
* Returns confirm form for deleting experience
*/
function abjs_experience_delete_confirm_form($form, &$form_state, $eid) {
  $form = array();
  $experience_result = db_query('SELECT name FROM {abjs_experience} WHERE eid = :eid', array(':eid' => $eid));
  if (!empty($experience_result)) {
    $form['eid'] = array('#type' => 'value', '#value' => $eid);
    $experience = $experience_result->fetchObject();
    $form['name'] = array('#type' => 'value', '#value' => $experience->name);
    return confirm_form($form,
      t('Are you sure you want to delete this experience: %name?', array('%name' => $experience->name)),
      'admin/config/abjs/experiences',
      t('This action cannot be undone.'),
      t('Delete'),
      t('Cancel')
    );
  }
  return $form;
}

/**
* Deletes experience from experience table, as well as all test-experience rows for this experience
*/
function abjs_experience_delete_confirm_form_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    db_delete('abjs_experience')
      ->condition('eid', $form_state['values']['eid'])
      ->execute();
    db_delete('abjs_test_experience')
      ->condition('eid', $form_state['values']['eid'])
      ->execute();

    drupal_set_message(t('Experience "%name" has been deleted.', array('%name' => $form_state['values']['name'])));
  }

  $form_state['redirect'] = 'admin/config/abjs/experiences';
}


//-------------End Experience functions-----------------//
