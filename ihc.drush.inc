<?php
/**
 * @file ihc.drush.inc
 *
 * Islandora Health Check Drush components.  The IHC module is a set of
 * Drush commands and supporting functions designed to report the 'health' of
 * a group of objects in an Islandora (Fedora) repository.  The commands all
 * have similar structure and all operate within one namespace at a time.
 *
 */

/**
 * Report columns are:
 *   A =  1 = PID Number                --Always reported!
 *   B =  2 = Full PID                  --Always reported!
 *   C =  3 = Object label (title)      --Always reported!
 *   D =  4 = CModels                                           --Standard report
 *   E =  5 = Parent (Collection, ConstituentOf or PageOf)      --Standard report
 *   F =  6 = OBJ MIME type                                     --Standard report
 *   G =  7 = Constituents (from RI)
 *   H =  8 = Children (not from RI)
 *   I =  9 = Object 'created' date                             --Standard report
 *   J = 10 = Object 'modified' date                            --Standard report
 *   K = 11 = isPartOf MODS Relations
 *   L = 12 = No. of DC.descriptions
 *   M = 13 = Object's OBJ size                                 --Standard report
 *   N = 14 = Object's Handle (HDL)
 *   O = 15 = RELS-EXT isViewableByRole value                   --Standard report
 *   P = 16 = Creators (extension)
 *   Q = 17 = CompoundTN status
 *   R = 18 = MODS dateCreated (Index Date)
 *
 * A 'standard' (default) report will include --reportColumns=ABCDEFIJMO
 * A 'special' report will include --reportColumns=ABCDGHNPQ
 *
 * Recently retired...
 *   J = 10 = Missing Datastreams ----- Required DSIDs which are missing
 *   K = 11 = MODS local ID (was OLD Original Content Datastreams)
 */

/**
 * Some available drush functions...
 *
 * drush_log('Log an event using drush', 'warning');
 * drush_set_error('Set an error with drush.');
 * dt('Translate strings with drush');
 * drush_print('Print to command line with drush');
 * drush_print_table($rows, TRUE); //print a command line table with drush
 * drush_confirm('Are you sure you want to continue?', $indent = 0); //Add drush confirmation
 *
 */

/** Callbacks */

/**
 * Callback function for drush ihcModReset.
 *
 * This callback resets the timestamp used to identify newly modified objects.
 *
 */
function drush_ihc_ihcModReset($pid) {
  $start = drush_get_option('modAfter', '2000-01-01T00:00:00Z');
  list($ns, $pidn) = explode(':', $pid);
  $functionStamp = 'drush_ihc_ihcCollect_'.$ns;
  variable_set($functionStamp, $start);
  $functionStamp = 'drush_ihc_ihcAnalyze_'.$ns;
  variable_set($functionStamp, $start);
  drush_log("ihcCollect and ihcAnalyze modAfter target date reset to '$start'.", 'status');
  return;
}

/**
 * Callback function for drush ihcCollect.
 *
 * This callback runs ihc_drush_collect_object_health repeatedly over a specified range of PIDs to collect necessary
 * information for subsequent analysis and reports.
 *
 * @param string $first
 *   First PID, with or without a 'namespace:' prefix, to process.
 */
function drush_ihc_ihcCollect($first) {
  module_load_include('inc', 'idu', 'object_record');

  list($ns, $pidn) = explode(':', $first);
  $functionStamp = __FUNCTION__.'_'.$ns;

  $modAfter = drush_get_option('modAfter', NULL);
  if (is_null($modAfter)) {
    $lastOp = variable_get($functionStamp, '1999-12-31T23:59:59Z');
  } else {
    $lastOp = $modAfter;
  }
  $start = date('Y-m-d\TH:i:s\Z');

  if (!$pids = ihc_drush_prep($first, $lastOp)) { return; }     // Get the PIDs and initialize the loop..
  $range = count($pids);
  icu_drush_print_progress('ihcCollect');

  // Loop through all the collected objects.
  $i = 0;
  foreach ($pids as $pid) {
    if ($follow = ihc_drush_collect_object_health($pid)) {
      foreach ($follow as $f) { ihc_drush_collect_object_health("$ns:$f"); }            // Git 'er done!
    }
    ihc_drush_post_to_file("ihcCollect: $pid complete.", NULL, 'public://ihcCollect_progress.log');
    icu_drush_print_progress('ihcCollect', $i++, $range);
  }

  // Done.
  $finish = date('H:i:s');
  drush_log("Completed ihcCollect at $finish. Use 'drush ihcA' to analyze and 'drush ihcR' to report it.", 'status');
  variable_set($functionStamp, $start);
}

/**
 * Callback function for drush ihcAnalyze.
 *
 * This callback runs ihc_.... repeatedly over a specified range of collected PID info to
 * perform analysis for subsequent reports.  This function may also append fhcF or fhcS
 * commands in public://ihcPendingOps.sh or other path named in --script.
 *
 * @param string $first
 *   First PID, with or without a 'namespace:' prefix, to process.
 */
function drush_ihc_ihcAnalyze($first) {
  module_load_include('inc', 'idu', 'object_record');

  list($ns, $pidn) = explode(':', $first);
  $functionStamp = __FUNCTION__.'_'.$ns;

  $modAfter = drush_get_option('modAfter', NULL);
  if (is_null($modAfter)) {
    $lastOp = variable_get($functionStamp, '1999-12-31T23:59:59Z');
  } else {
    $lastOp = $modAfter;
  }
  $start = date('Y-m-d\TH:i:s\Z');

  if (!$pids = ihc_drush_prep($first, $lastOp)) { return; }     // Get the PIDs and initialize the loop..
  $range = count($pids);
  icu_drush_print_progress('ihcAnalyze');

  // Loop through all the collected objects.
  $i = 0;
  foreach ($pids as $pid) {
    icu_drush_print_progress('ihcAnalyze', $i++, $range);

    // Check for a valid object with data..
    if ($row = or_get($pid)) {
      if ($codes = ihc_drush_apply_rules($row)) {    // apply the rules!
        or_set_codes($pid, $codes);                  // ...and the codes those rules create.
      }
    }
    ihc_drush_post_to_file("ihcAnalyze: $pid complete.", NULL, 'public://ihcAnalyze_progress.log');
  }

  // Done.
  $finish = date('H:i:s');
  drush_log("Completed ihcAnalyze at $finish. Use 'drush ihcR' to report it.", 'ok');
  variable_set($functionStamp, $start);
}

/**
 * Callback function for drush ihcReport.
 *
 * This callback runs ihc_report_object_health repeatedly over a specified range of PIDs to retrieve necessary
 * information and produce a corresponding Excel worksheet displaying the collection and analysis results.
 *
 * @param string $first
 *   First PID, with a 'namespace:' prefix, to process.
 * @param string $file
 *   Optional file name (in public://) of the XLSX file to be written.  If NULL the --xlsx option
 *   is used.
 */
function drush_ihc_ihcReport($first, $file=NULL) {
  module_load_include('inc', 'idu', 'object_record');

  list($ns, $pidn) = explode(':', $first);
  if (!$pids = ihc_drush_prep($first)) { return; }     // Get the PIDs and initialize the loop..
  $range = count($pids);

  // Check for an --awstats option.  If present, skip normal processing and return only the AWSTATS.
  $awstats = drush_get_option('awstats', NULL);
  $headers = array( );

  // Check for a --reportColumns option.
  $reportCol = drush_get_option('reportColumns', 'ABCDEFGHIJKLMNOPQR');
  if ($reportCol === 'standard') {
    $reportCol = 'ABCDEFIJMOR';
  } else if ($reportCol === 'special') {
    $reportCol = 'ABCDGHNPQ';
  } else if ($reportCol === 'full') {
    $reportCol = 'ABCDEFGHIJKLMNOPQR';
  }
  $reportColumns = str_split($reportCol, 1);

  // If this is not AWSTATS, prep and print some header info.
  if (is_null($awstats)) {                       // Check for other options...
    $xlsx = drush_get_option('xlsx', NULL);
    if (is_null($xlsx) && is_null($file)) {
      $path = variable_get('icg_xlsxFile', 'public://HEALTH_CHECK.xlsx');
    } else if ($xlsx) {
      $path = "public://$xlsx";
      variable_set('icg_xlsxFile', $path);
    } else if ($file) {
      $path = "public://$file";
      variable_set('icg_xlsxFile', $path);
    }

    drush_log("XLSX file path is: '$path'.", 'status');

    // Does the file already exist?  If yes, delete it.
    if (is_file($path)) {
      if (!unlink($path)) {
        die("Unable to delete existing '$path'!  ");
      }
    }

    // Git 'er done.
    $title = "Islandora Health Check - " . date('Y-m-d H:i:s');
    $description = "Islandora Health Check of Namespace '$ns' for Office 2007 XLSX, generated using PHP classes.";

    $param = array('format' => 'xlsx', 'creator' => 'Mark McFate',
      'title' => $title, 'subject' => "Islandora Health Check", 'description' => $description);

    // Prep the column headers.
    $allHeaders = ihcColumns(1, NULL, FALSE);
    $alphabet = range('A','Z');

    foreach ($allHeaders as $c => $header) {
      if (in_array($alphabet[$c], $reportColumns)) {
        $headers[] = $header;
      }
    }
  }

  icu_drush_print_progress('ihcReport');   // Initialize the progress bar

  // If AWSTATS was asked for...

  if (!is_null($awstats)) {
    $objCount = $visits6 = $visits7 = $downloads = 0;
    $p = 0;
    foreach ($pids as $pid) {
      if ($row = or_get($pid)) {
        $objCount++;
        list($v6, $v7, $d) = ihc_return_awstats($pid);
        $visits6 += $v6;
        $visits7 += $v7;
        $downloads += $d;
      }
      ihc_drush_post_to_file("ihcReport: $pid AWSTATS is complete.", NULL, 'public://ihcReport_progress.log');
      icu_drush_print_progress('ihcReport', $p++, $range);
    }
    $objects = format_plural($objCount, "The specified object was visited an estimated", "The @count specified objects were visited an estimated", array('count' => $objCount));
    $v6 = format_plural($visits6, "once in version 6 with", "@count times in version 6 with", array('count' => $visits6));
    $d6 = format_plural($downloads, "one download.", "@count downloads.", array('count' => $downloads));
    $v7 = format_plural($visits7, "once in version 7 thus far.", "@count times in version 7 thus far.", array('count' => $visits7));
    drush_log("ihcReport: $objects $v6 $d6", 'status');
    drush_log("ihcReport: $objects $v7", 'status');
    return;
  }

  //---------------------------------------------------------------------------------
  // OK, we are printing a repository report and not AWSTATS... Do it.
  // Read the data, apply formatting and save it in $sheet.

  $sheet = array( );
  $i = $p = 0;
  $pidsR = array_reverse($pids);    // invert the PIDs order to get the newest on top.
  foreach ($pidsR as $pid) {
    if ($row = or_get($pid)) {
      $codes = or_get_codes($pid);
      $sheet[$i] = ihc_drush_fill_sheet($row, $codes, $reportColumns);
      ++$i;
    }
    ihc_drush_post_to_file("ihcReport: $pid complete.", NULL, 'public://ihcReport_progress.log');
    icu_drush_print_progress('ihcReport', $p++, $range);
  }

  // Save Excel 2007 file
  $xlsxName = variable_get('icg_xlsxFile');
  $path = drupal_realpath($xlsxName);
  drush_log("ihcReport: Beginning PHPExcel export to $path.", 'status');
  $result = phpexcel_export($headers, $sheet, $path, $param);

  if ($result === PHPEXCEL_SUCCESS) {
    $url = file_create_url($xlsxName);
    $_SESSION['ihc_xlsx_file'] = $xlsxName;
    drupal_set_message(t('Saved XLSX file as %xlsxName (accessible via !url, uri=<span id="uri">@uri</span>)', array('%xlsxName' => $xlsxName, '@uri' => $xlsxName, '!url' => l(t('this URL'), $url, array("attributes" => array("target" => "_blank"))))));
  } else {
    $msg = "Error writing the XLSX file. Result code = '$result'.";
    drupal_set_message(t($msg), 'error');
  }

  return;
}

/**
 * Callback function for the drush ihcQuick command.
 *
 * This callback runs a complete sequence of ihcC, ihcA and ihcR commands on the specified range of PIDs
 * producing a pair of health check reports with default names of 'ihcQ_<namespace>_standard.xlsx' and
 * 'ihcQ_<namespace>_special.xlsx'.
 *
 * @param $first
 *   First PID number to process.
 */
function drush_ihc_ihcQuick($first) {
  list($ns, $pid) = explode(':', $first);
  drush_ihc_ihcCollect($first);
  drush_ihc_ihcAnalyze($first);

  // Run a 'standard' report.
  drush_set_option('reportColumns', 'standard');
  $name = "ihcQ_".$ns."_standard.xlsx";
  $fileName = drush_get_option('xlsx', $name);
  drush_ihc_ihcReport($first, $fileName);

  // Run a 'special' report.
  drush_set_option('reportColumns', 'special');
  $name = "ihcQ_".$ns."_special.xlsx";
  $fileName = drush_get_option('xlsx', $name);
  drush_ihc_ihcReport($first, $fileName);
}

/** Command definition and associated functions */

/**
 * Implements hook_drush_command().
 */
function ihc_drush_command( ) {
  $items = array();

  $items['ihcModReset'] = array(
    'description' => dt('Reset --modAfter target object date.'),
    'arguments'   => array(
      'first' => dt("First PID to process.  The namespace portion of this PID defines the default primary namespace.  This parameter may be specified as 'namespace:first-last' in place of the --repeat option."),
    ),
    'options' => array(
      'modAfter' => dt("Sets a new YYYY-mm-ddTHH:ii:ssZ timestamp string. Default is '2000-01-01T00:00:00:Z'."),
    ),
    'examples' => array(
      'Example' => 'drush -u 1 ihcModReset grinnell:1 --modAfter=2015-09-01T00:00:00Z',
    ),
    'aliases' => array('ihcM'),
  );

  $items['ihcCollect'] = array(
    'description' => dt('Collect IHC info for a range of PIDs.'),
    'arguments'   => array(
      'first' => dt("First PID to process.  The namespace portion of this PID defines the default primary namespace.  This parameter may be specified as 'namespace:first-last' in place of the --repeat option."),
    ),
    'options' => array(
      'repeat'   => dt("Specifies the number of consecutive PIDs following 'first' to be processed.  Default is 'all'."),
      'follow'   => dt('If specified, the known constituents of compoundCModel objects are processed following their parent.'),
      'modAfter' => dt("If set with a YYYY-mm-ddTHH:ii:ssZ timestamp string, then only objects modified after said local time are considered. Default is the last operation date/time."),
    ),
    'examples' => array(
      'Example' => 'drush -u 1 ihcCollect grinnell:1 --repeat=all',
    ),
    'aliases' => array('ihcC'),
  );

  $items['ihcAnalyze'] = array(
    'description' => dt('Analyze existing Islandora Health Check data for a specified range of existing PID data.'),
    'arguments'   => array(
      'first' => dt('First PID to process.  The namespace portion of this PID defines the default primary namespace.'),
    ),
    'options' => array(
      'repeat'   => dt("Specifies the number of consecutive PIDs following 'first' to be processed.  Default is 'all'."),
      'script'   => dt('The path of a new or existing bash script to accept fhcF and fhcS commands for follow-up operations.'),
      'modAfter' => dt("If set with a YYYY-mm-ddTHH:ii:ssZ timestamp string, then only objects modified after said local time are considered. Default is the last operation date/time."),
    ),
    'examples' => array(
      'Example' => 'drush ihcAnalyze grinnell:100 --repeat=27 --script=public://script.sh',
    ),
    'aliases' => array('ihcA'),
  );

  $items['ihcReport'] = array(
    'description' => dt('Report existing Islandora Health Check data and analysis results to create an XSLS report from a specified range of existing PID data.'),
    'arguments'   => array(
      'first' => dt('First PID to process.  The namespace portion of this PID defines the default primary namespace.'),
    ),
    'options' => array(
      'repeat'      => dt("Specifies the number of consecutive PIDs following 'first' to be processed.  Default is 'all'."),
      'xlsx'        => dt('The name of the XLSX file to be created.'),
      'awstats'     => dt("Specifies the return of only AWSTATS (access) statistics for the target object(s)."),
      'reportColumns' => dt("A string of up to 10 letters representing the columns to be reported, or 'special' to represent columns 'ABCDGHNPQ'.  Default is 'standard' representing 'ABCDEFIJMO'."),
    ),
    'examples' => array(
      'Example' => 'drush ihcReport grinnell:100 --repeat=5 --xlsx=HEALTH_CHECK_Nov-10.xlsx',
    ),
    'aliases' => array('ihcR'),
  );

  $items['ihcQuick'] = array(
    'description' => dt('Collect, analyze, and report Islandora Health Check info for one or more PIDs with output directed to an XLSX file.'),
    'arguments' => array(
      'first'    => dt('First PID to process.  The namespace portion of this PID defines the default primary namespace.'),
    ),
    'options' => array(
      'repeat'      => dt("Specifies the number of consecutive PIDs following 'first' to be processed.  Default is 'all'."),
      'xlsx'        => dt('The name of the XLSX file to be created.  Default is DG_Quick.xlsx.'),
      'reportColumns' => dt("A string of five letters representing the columns to be reported along with A-C and I-J.  Default is 'DEFGH'."),
      'modAfter'    => dt("If set with a YYYY-mm-ddTHH:ii:ssZ timestamp string, then only objects modified after said local time are considered. Default is the last operation date/time."),
   ),
    'examples' => array(
      'Example' => 'drush ihcQuick grinnell:100 --repeat=10',
    ),
    'aliases' => array('ihcQ'),
  );

  return $items;
}

/**
 * Implements hook_drush_help().
 */
function ihc_drush_help($command) {
  switch ($command) {
    case 'drush:ihcCollect':
      return dt('Collects health check data for a range of existing PIDs for subsequent Islandora Health Check reports.');
    case 'drush:ihcAnalyze':
      return dt('Analyzes health check data for a range of existing PIDs for subsequent Islandora Health Check reports.');
    case 'drush:ihcReport':
      return dt('Report AWSTATS for an object or existing Islandora Health Check data with analysis formatting to an XLSX report file.');
    case 'drush:ihcQuick':
      return dt('Collects, analyzes, and reports health check data for a range of existing PIDs with output to default named files.');
  }
}

/**
 * Examine common ihcX command arguments/options and set control variables for subsequent processing.
 *
 * @param string $pidSpec
 *   The PID argument as specified in the ihcX command.
 * @param null|string $lastOp
 *   The date/time of the last successful operation.  Only objects modified after this date should be processed.
 * @return array
 *   An array of valid PIDs from the specified range.
 *
 */
function ihc_drush_prep($pidSpec, $lastOp='2000-01-01T00:00:00Z') {
  module_load_include('inc', 'idu', 'icu_drush');
  $last = $pidn = $range = $namespace = 0;
  return icu_drush_prep($pidSpec, $last, $pidn, $range, $namespace, $lastOp);
}

/** Remaining functions in alphabetical order */

/**
 * Add a child object (page or constituent) to an existing list of children.
 *
 * @param string $kids
 *   Existing '[count] list' string.
 * @param integer $newKid
 *   New ID to be added, if necessary.
 * @return bool|string
 *   Returns an updated '[count] list' or FALSE for no change.
 */
function ihc_drush_add_child_to_list($kids, $newKid) {
  module_load_include('inc', 'idu', 'icu');

  $children = array( );

  // Burst $kids into a sorted array of existing $children.
  list($count, $content) = icu_get_count_and_content($kids);
  $childs = explode(',', $content);
  foreach ($childs as $child) {
    if (preg_match('/(\d+)-(\d+)/', $child, $range)) {   // we have a range of values
      for ($i=intval($range[1]); $i<=intval($range[2]); $i++) {
        $children[] = "$i";
      }
    } else {
      $children[] = $child;
    }
  }
  asort($children);

  // See if $newKid is already in the array.  If not, add it.
  if ($count == 0) {                       // first child...simply add it
    return "[1] $newKid";
  } else if (in_array($newKid, $children)) {  // child already in the list, do nothing
    return FALSE;
  } else {                                 // child not already present, unless it falls in an existing range
    $children[] = $newKid;
  }
  asort($children);

  // Now rebuild the list in sorted order.
  $previous = $inRange = $rangeEnd = 0;
  $count = count($children);
  $updated = "[$count] ";

  foreach ($children as $child) {
    if (intval($child) != $previous + 1) {  // $child is not part of a range...just append it.
      if ($inRange) {                 // ...first check if we are in a range..close it.
        $updated .= $rangeEnd . ',';
        $inRange = FALSE;
      }
      $updated .= $child . ',';
    }
    else {
      if ($inRange) {            // $child is part of an active range...make it the end of the active range.
        $rangeEnd = $child;
      }
      else {                     // $child is part of a range but none is active...start one.
        $updated = trim($updated, ", ").'-';
        $inRange = TRUE;
        $rangeEnd = $child;
      }
    }
    $previous = intval($child);
  }

  // All done with children, if the last one was in a range, close it.
  if ($inRange) {
    $updated .= $rangeEnd;
  }

  // Trim the string and return it.
  return rtrim($updated, ", ");

}

/**
 * Apply assessment rules to one row ($columns) of data.
 *
 * @param array $columns
 *    The row of data in $colName (an uppercase letter) => $data to be analyzed.
 * @return array
 *   An associative array of format codes in $column => $code form.
 */
function ihc_drush_apply_rules(array $columns) {
  module_load_include('inc', 'idu', 'icu');

  $row = array( );
  $isGreen = TRUE;

  // Get the object...
  $pid = $columns['B'];
  list($ns, $pidn) = explode(':', $pid);

  if ($pid === '*None*') {
    $row['B'] = 'SOFT';
    return $row;
  }

  /* Not skipped, eh?  Check the object state and report.
  if (!empty($columns['C'])) {
    list($fixed, $state) = icu_get_count_and_content($columns['C']);
    if ($state != "A") {
      $row['B'] = 'SOFT';
      return $row;
    }
  } */

  // Ok, we have an active object...get busy.  Start by reporting some basic info.
  $row['B'] = 'BOLD';

  /* Report the total number of datastreams.
  if (!empty($columns['D'])) {
    $nDS = $columns['D'];
    if ($nDS > 19) { $row['D'] = 'YELLOW'; }
  } */

  // Check the CModels.  If no RELS-EXT...report that here.
  $cModel = FALSE;
  if (!empty($columns['D'])) {
    $cm = $columns['D'];
    if ($cm === '*No RELS-EXT*') {
      $row['D'] = 'RED';
      $isGreen = FALSE;
      return;
    } else {
      list($nCM, $cModel) = icu_get_count_and_content($cm);
      if ($nCM > 2) {
        $row['D'] = 'RED';
        $isGreen = FALSE;
      } else if ($nCM != 1 ) {
        $row['D'] = 'YELLOW';
      }
    }
  }

  // If this is a compound object with no children...flag it.
  if ($cModel === 'compoundCModel') {
    list($nChild, $kids) = icu_get_count_and_content($columns['G']);
    if ($nChild === 0) {
      $row['D'] = $row['G'] = 'YELLOW';
      $isGreen = FALSE;
    }
    list($nChild, $kids) = icu_get_count_and_content($columns['H']);
    if ($nChild === 0) {
      $row['D'] = $row['H'] = 'YELLOW';
      $isGreen = FALSE;
    }
  }

  // Report all the parent (isMemberOfCollection, isConstituentOf and
  // isPageOf) relationships.
  // If the parent is an object in the same namespace, determine if that parent exists.  If not, we have an orphan.

  if (!empty($columns['E'])) {
    list($nRel, $parent) = icu_get_count_and_content($columns['E']);
    if ($nRel < 1 || $nRel > 2) {    // zero or more than two parents, not good.
      $row['E'] = 'RED';
      $isGreen = FALSE;
    } else {
      $parents = explode(',', $parent);
      foreach ($parents as $parent) {
        list($pns, $ppid) = explode(':', trim($parent));
        if (is_numeric($ppid) && $pns === $ns) {
          if (!or_get("$pns:$ppid")) {
            $row['E'] = 'RED';
            $isGreen = FALSE;
          }
        }
      }
    }
  }

  // Check the OBJ datastream.

  if (!empty($columns['F']) && $cModel != 'binaryObjectCModel') {
    $OBJMimes = icu_OBJMimes( );
    $mime = $columns['F'];
    if (!isset($OBJMimes[$cModel])) {
      $row['F'] = 'YELLOW';               // better check our icu_OBJMimes list!
    } else if (strpos($OBJMimes[$cModel], '*')) {
      $row['F'] = 'GREEN';
    } else {
      $expected = $OBJMimes[$cModel];
      if ($mime != $expected) {
        $row['F'] = 'RED';                // red if not the expected type
        $isGreen = FALSE;
      }
    }
  }

  /* Missing datastreams...red for ! missing required DS, yellow otherwise.
  // Except for those listed in $allowed below.

  static $allowed = array('[1] TECHMD', '[1] RAW_HOCR', '[1] HOCR', '[2] RAW_HOCR HOCR');

  if ($columns['J'] != 'None') {
    $missing = $columns['J'];
    if (strlen($missing) > 0 ) {
      if (strstr($missing, '!')) {
        $row['J'] = 'RED';
        $isGreen = FALSE;
      } else if (!in_array($missing, $allowed)) {
        $row['J'] = 'YELLOW';
      } else {
        $row['J'] = 'NORMAL';
      }
    }
  } else {
    $row['J'] = 'NORMAL';
  } */

  /* Check for old content issues.
  if (!empty($columns['K'])) {
    if (strlen($columns['K']) < 1) {
      $row['K'] = 'YELLOW';           // No old content?
      $count = 0;
    }

    // If the OLD content count is greater than 1 and this is not a compoundCModel or bookCModel...big problem!
    if ($count > 1 && !in_array(array('compoundCModel','bookCModel'), $cModel)) {
      $row['E'] = 'RED';
      $isGreen = FALSE;
    }
  } */

  // Check for bad isPartOf references and set cell yellow if any are found.

  if ($columns['K'] != 'None') {
    list($nPart, $content) = icu_get_count_and_content($columns['K']);
    $parts = explode(',', $content);
    if ($nPart != count($parts)) {          // number of reported isPartOf is inconsistent
      $row['K'] = 'YELLOW';
    } else {
      foreach ($parts as $part) {
        if (preg_match('/^grinnell\:/', trim($part))) {  // this is an old grinnell: object reference.  Not right!
          $row['K'] = 'YELLOW';
        } else if (preg_match('/^grinnell\-auth\:/', trim($part))) {   // this is an old grinnell-auth: object reference.  Not right!
          $row['K'] = 'YELLOW';
        } else if (!preg_match('/.+\:.+/', trim($part))) {  // no colon present...this isn't even a proper reference!
          $row['K'] = 'RED';
        }
      }
    }
  } else {
    $row['K'] = 'NORMAL';
  }

  // Check for NO dc.description.  Unless this is a pageCModel we have a problem.

  $nDC = 0;
  if (!empty($columns['L'])) { list($nDC, $content) = icu_get_count_and_content($columns['L']); }
  if ($nDC < 1 && $cModel != 'pageCModel') {
    $row['L'] = 'YELLOW';
    $isGreen = FALSE;
    $command = "drush iduF $pid DCTransform";
    ihc_drush_post_to_file("Follow-up for $pid. Transform MODS-to-DC.", $command);
  } else {
    $row['L'] = 'NORMAL';
  }

  // Check for ZERO (or very small) size in the OBJ datastream.

  if (!empty($columns['M'])) {
    $size = intval($columns['M']);
    if ($size < 2000) {
      $row['M'] = 'RED';
      $isGreen = FALSE;
    }
  } else {
    $row['M'] = 'NORMAL';
  }

  // Get the object's cModel (and CM count).
  list($nCM, $cModel) = icu_get_count_and_content($columns['D']);

  // Check for a Handle or 'None'.
  $row['N'] = 'NORMAL';
  if (!empty($columns['N'])) {
    $handle = $columns['N'];
    if ($handle === "None" && $cModel != "pageCModel") {
      $row['N'] = 'RED';
      $isGreen = FALSE;
    }
  }

  // Check for an isViewableByRole or 'None'.
  $row['O'] = 'NORMAL';
  if (!empty($columns['O'])) {
    $val = $columns['O'];
    if ($val != "None" ) {
      $row['O'] = 'RED';
      $isGreen = FALSE;
    }
  }

  // Check for Creators and dateCreated.
  $row['P'] = $row['R'] = 'NORMAL';
  if (!empty($columns['P'])) {
    $val = $columns['P'];
    if ($val === "None" && $cModel != 'pageCModel') {
      $row['P'] = 'ORANGE';
    }
  }
  if (!empty($columns['R'])) {
    $val = $columns['R'];
    if ($val === "None" && $cModel != 'pageCModel') {
      $row['R'] = 'ORANGE';
    } elseif (strlen($val) != 4) {
      $row['R'] = 'RED';
    }
  }

  // Check for Missing TN and/or CompoundTN.
  $row['Q'] = 'NORMAL';
  if (!empty($columns['Q'])) {
    $val = "_".$columns['Q'];
    if (stripos($val, 'missing') > 0) {
      if (stripos($val, 'CompoundTN') > 0) { $row['Q'] = 'ORANGE'; }
      if (stripos($val, ' TN') > 0) { $row['Q'] = 'RED'; }
      $isGreen = FALSE;
    }
  }

  // Mark special circumstances in ORANGE.
  //

  // 1) Any object with a mis-match of legitimate children and constituents.  These should be RED!
  // But consider these conditions...
  //   a) An object with NO constituents is legit if it had only one old content datastream and is not tagged as compound.
  //   b) A pageCModel object can have NO constituents.
  //   c) A bookCModel should have more than one constituent and the same number from the RI and non-RI count..
  //   d) A non-compound object with multiple content datastreams where the number of $nG or $nH does not match.
  //        -- convert the object to compound and sort the rest out later

  list($nCol, $collection) = icu_get_count_and_content($columns['E']);     // collections
  list($nG, $constituents) = icu_get_count_and_content($columns['G']);     // constituents from the RI
  list($nH, $consH) = icu_get_count_and_content($columns['H']);            // constituents not from the RI

  if ($nG != $nH) {   // number of non-RI constituents != number of RI constituents

    // Check for case a.
    if ($nG == 0) {             // this condition is OK, except if the $cModel is compoundCModel.
      if ($cModel === 'compoundCModel') {
        $row['D'] = $row['G'] = 'RED';
        $isGreen = FALSE;
      }

    // Check for case b.
    } else if ($cModel === 'pageCModel') {
      if ($nG > 0) {
        $row['D'] = $row['G'] = 'RED';
        $isGreen = FALSE;
      }


      // Check for case C.
    } else if ($cModel === 'bookCModel') {
      if ($nG < 2 || ($nG != $nH)) {
        $row['D'] = $row['G'] = $row['H'] = 'RED';
        $isGreen = FALSE;
      }

    // Check for case D.
    } else if ($cModel != 'compoundCModel') {
      /* if ($nK > 1) {
          $command = "drush iduF $pid ChangeText --find=$cModel --replace=compoundCModel --dsid=RELS-EXT";
          ihc_drush_post_to_file("Follow-up for $pid. Special Rule 1d.", $command);
          $row['E'] = $row['K'] = $row['H'] = $row['I'] = 'RED';
          $isGreen = FALSE;
        } */

    // All others are suspect.
    } else {
      $command = "drush iduM $pid --replace --collection=$collection --workflow=published";
      ihc_drush_post_to_file("Follow-up for $pid. Special Rule 1.", $command);
      $row['G'] = $row['H'] = 'RED';
      $isGreen = FALSE;
    }
  }

  // 2) A non-compound, non-book object with constituents!  If the object has an OBJ and only one
  //    legitimate content datastream, then we need to delete the constituents.
  //

  if ($nG > 0 && $cModel != 'compoundCModel' && $cModel != 'bookCModel') {
    $row['D'] = 'ORANGE';
    $isGreen = FALSE;

    if ($nCM == 1) {
      $OBJMimes = icu_OBJMimes();
      if ($columns['F'] === $OBJMimes[$cModel]) {   // the object has the right OBJ type..good-to-go for deletion of the bogus kids
        $parts = explode(',' , $constituents);
        foreach ($parts as $part) {
          ihc_drush_post_to_file("Follow-up for $pid. Special Rule 2.", NULL);
          if (preg_match('/^(\d+)\-(\d+)$/', $part, $matches)) {                        // got a range of objects
            $repeats = intval($matches[2]) - intval($matches[1]);
            $command = "drush iduF $ns:$matches[1] Purge --repeat=$repeats";
          } else {                                                                     // got a single object
            $command = "drush iduF $ns:$part Purge";
          }
          ihc_drush_post_to_file(NULL, $command);
        }
      }
    }
  }

  // 3) A compound object with only one legitimate constituent!

  if ($cModel === 'compoundCModel') {
    if ($nG == 1) {
      $row['D'] = 'ORANGE';
      $isGreen = FALSE;
      ihc_drush_post_to_file("Follow-up for $pid. Special Rule 3.", "drush ihcS SingleCompound $pid $ns:$constituents");
    }
  }

  // If $isGreen is TRUE, set the full PID to green!
  if ($isGreen) { $row['B'] = 'GREEN'; }

  return $row;
}

/**
 * Collect object health data for a single object.
 *
 * @param $pid
 * @return array|bool
 */
function ihc_drush_collect_object_health($pid) {
  static $mods_ns = 'mods';
  static $mods_nsU = 'http://www.loc.gov/mods/v3';

  $toFollow = array( );
  list($ns, $pidn) = explode(':', $pid);
  $r = intval($pidn);

  try {
    $object = ihc_repo()->getObject($pid);
  } catch (Exception $e) {
    return FALSE;                 // for any exception...skip this PID
  }

  // Not skipped, eh?
  $state = $object->state;
  $data = array('A'=>$r, 'B'=>$pid);
  if ($state != "A") {
    or_set($pid, $data);
    return FALSE;
  }

  // Report the object->label (title).
  $data['C'] = substr($object->label, 0, 255);

  /* Report the total number of datastreams.
  $nDS = count($object);
  $data['D'] = $nDS; */

  // Report the object's CModels.
  $rels = $object->relationships->get(FEDORA_MODEL_URI, 'hasModel');
  $nCM = count($rels);
  if ($nCM != 1) {
    $cModel = 'Multiple!';
    $cModelText = "[$nCM] ";
    foreach ($rels as $rel) { $cModelText .= substr(strstr($rel['object']['value'], ':'), 1).','; }
  } else {
    $cModel = substr(strstr($rels[0]['object']['value'], ':'), 1);
    $cModelText= "[$nCM] $cModel";
  }
  $data['D'] = trim($cModelText, " ,");

  // Fetch all the governing isMemberOfCollection, isConstituentOf or
  // isPageOf relationships.

  $parents = '';
  $rels = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isConstituentOf');
  if (count($rels) == 0) { $rels = $object->relationships->get(ISLANDORA_RELS_EXT_URI, 'isPageOf'); }

  $nCM += count($rels);
  foreach ($rels as $rel) {
    $parent = $rel['object']['value'];
    $parents .= "$parent,";

    // Add this constituent to its parent in $data['H']
    if ($pData = or_get($parent)) {
      $pI['H'] = $pData['H'];       // update only cell 'H'
      if (!empty($pI['H'])) {       // add this to the list of kids
        if ($updated = ihc_drush_add_child_to_list($pI['H'], $r)) { $pI['H'] = $updated; }
      } else {
        $pI['H'] = "[1] $r";
      }
      or_set($parent, $pI);
    }
  }

  $rels = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOfCollection');
  $nCM += count($rels);
  foreach ($rels as $rel) { $parents .= $rel['object']['value'].','; }

  $data['E'] = "[$nCM] ".trim($parents, ' ,');
  $data['F'] = $data['M'] = NULL;

  // Now do some detailed datastream collection by MIME type.

  $DSMimes = icu_DSMimes( );
  $others = array( );
  $badMimes = '';

  foreach($object as $ds) {
    if ($ds->state != 'A') { continue; }   // skip the datastream if it's inactive or deleted
    $dsid = trim($ds->id);
    $mime = trim($ds->mimetype);
    if ($dsid === 'OBJ') {               // always report the OBJ MIME type AND size!
      $data['F'] = "$mime";
      $data['M'] = $ds->size;
    } else if (!empty($DSMimes[$dsid])) {
      $expected = $DSMimes[$dsid];          // Is this a 'common' DSID?
      if ($mime != $expected) { $badMimes .= "$dsid:$mime "; }  // otherwise, is the MIME type as expected?
    } else if (empty($others[$mime])) {
      $others[$mime] = "$dsid";
    } else {
      $others[$mime] .= ",$dsid";
    }
  }

  // Get constituents from the RI.

  $data['G'] = NULL;

  // If this is a book... fetch all the pagedContent belonging to it.
  if ($cModel === 'bookCModel') {
    $pages = islandora_paged_content_get_pages($object);
    foreach ($pages as $page => $info) {
      list($cns, $c) = explode(':', $page);
      if (!empty($data['G'])) {  // add this to the list of kids (pages)
        if ($updatedKids = ihc_drush_add_child_to_list($data['G'], $c)) { $data['G'] = $updatedKids; }
      } else {
        $data['G'] = "[1] $c";
      }
    }
  }

  $status = $data['Q'] = 'Not Applicable';

  // If this is a compound... fetch all the constituents belonging to it.
  if ($cModel === 'compoundCModel') {
    $follow = drush_get_option('follow', FALSE);
    $pages = islandora_compound_object_get_parts($object->id);
    sort($pages);
    foreach ($pages as $page) {
      list($cns, $c) = explode(':', $page);
      if ($follow) { $toFollow[] = $page; }
      if (!empty($data['G'])) {  // add this to the list of kids
        if ($updatedKids = ihc_drush_add_child_to_list($data['G'], $c))  { $data['G'] = $updatedKids; }
      } else {
        $data['G'] = "[1] $c";
      }
    }

    // And while we are here...check for TN and CompoundTN datastreams and report.
    if (isset($object['CompoundTN']) && isset($object['TN'])) {
      $status = 'OK';
    } else {
      $status = 'Missing ';
    }
    if (!isset($object['TN'])) { $status .= "TN "; }
    if (!isset($object['CompoundTN'])) { $status .= "CompoundTN "; }
    $data['Q'] = trim($status, ' ');
  }

  if (empty($data['G'])) { $data['G'] = '[0] None'; }

  /* Now report any missing datastreams.
  if ($missing = ihc_identify_missing_datastreams($object, $cModel)) {
    $data['I'] = $missing;
  } else {
    $data['I'] = 'None';
  } */

  /* Now count and report the OLD content datastreams.
  $count = 0;              // cell form:  [total] mime1:count1,mime2:count2
  if ($oldContent = ihc_drush_count_old_content($object, $count)) {
    $cell = '';
    foreach ($oldContent as $mime => $n) { $cell .= "$mime:$n,"; }
    $data['K'] = trim("[$count] $cell", " ,");    // Nothing special here.
  } else {
    $data['K'] = 'None';    // Nothing at all here!
  } */

  /* Fetch and report the object's local ID (from /mods:mods/mods:identifier[@type='local'])
  $cell = 'None';

  if (isset($object['MODS'])) {
    $mods = $object['MODS']->content;
    $xml = new SimpleXMLElement($mods);      // convert MODS content to searchable XML

    if ($node = $xml->xpath("/mods:mods/mods:identifier[@type='local']")) {
      foreach ($node as $n) {
        $cell = trim("$n");
        break;
      }
    }
  }
  $data['J'] = $cell; */

  // Fetch some $object data.  Namely the create and modified dates of the object.
  $foxml = ihc_repo( )->api->m->export($pid,
    array('format' => 'info:fedora/fedora-system:FOXML-1.1',
      'context' => 'migrate',
      'encoding' => 'UTF-8')
  );

  $xpath1 = "/foxml:digitalObject/foxml:objectProperties/foxml:property[@NAME='info:fedora/fedora-system:def/model#createdDate']";
  $xpath2 = "/foxml:digitalObject/foxml:objectProperties/foxml:property[@NAME='info:fedora/fedora-system:def/view#lastModifiedDate']";

  /* @VALUE="2012-06-04T12:35:36.507Z" */

  $xml = new DOMDocument;
  $xml->loadXML($foxml);
  $dx = new DOMXPath($xml);
  $dx->registerNamespace($mods_ns, $mods_nsU);

  // Created date from the FOXML
  $elements = $dx->query($xpath1);
  foreach ($elements as $element) {
    $data['I'] = $element->getAttribute('VALUE');
    break;
  }

  // Modified date from the FOXML
  $elements = $dx->query($xpath2);
  foreach ($elements as $element) {
    $data['J'] = $element->getAttribute('VALUE');
    break;
  }

  // Prep to search MODS
  if (isset($object['MODS'])) {
    $mods = $object['MODS']->content;
    $xml = new DOMDocument;
    $xml->loadXML($mods);
    $mx = new DOMXPath($xml);
    $mx->registerNamespace($mods_ns, $mods_nsU);
  }

  // Dump any/all MODS isPartOf relations that use identifiers (authorities).
  $content = $cell = '';
  if (isset($object['MODS'])) {
    $count = 0;
    if ($node = $mx->query('/mods:mods/mods:relatedItem[not(@displayLabel)]/mods:identifier')) {
      foreach ($node as $n) {
        ++$count;
        $v = $n->nodeValue;
        $content .= ", $v";
      }
      if ($count > 0) {
        $cell = "[$count] " . substr(trim($content, ' ,'), 0, 255);
      }
    }
  }
  if (strlen($cell) > 0) {
    $data['K'] = $cell;
  } else {
    $data['K'] = 'None';
  }

  // Indicate if this object has any DC.description elements.  If not, it probably needs
  // a new MODS-to-DC transform.
  $cell = '';
  if (isset($object['DC'])) {
    $dc = $object['DC']->content;
    $count = 0;
    $xml = new SimpleXMLElement($dc);      // convert DC content to searchable XML

    if ($node = $xml->xpath('/oai_dc:dc/dc:description')) {
      if (count($node) > 0) { $cell = "[".count($node)."]"; }
    }
  }
  if (strlen($cell) > 0) {
    $data['L'] = $cell;
  } else {
    $data['L'] = 'None';
  }

  // Report the object's handle (HDL) value from the MODS record.
  $cell = 'None';
  if (isset($object['MODS'])) {
    if ($node = $mx->query('/mods:mods/mods:identifier[@type="hdl"]')) {
      if (count($node) > 0) {
        foreach ($node as $n) {
          $cell = $n->nodeValue;
        }
      } else {
        $cell = 'None';
      }
    }
  }
  $data['N'] = $cell;

  // Report any RELS-EXT isViewableByRole values.
  $rels = $object->relationships->get(ISLANDORA_RELS_EXT_URI, 'isViewableByRole');
  $nR = count($rels);
  $values = '';
  if ($nR > 0 ) {
    foreach ($rels as $rel) {
      $values .= $rel['object']['value'] . ',';
    }
    $data['O'] = "[$nR] ".trim($values, ' ,');
  } else {
    $data['O'] = "None";
  }

  // Report Creators extensions and /mods:mods/mods:originInfo/mods:dateCreated or INDEX date.
  $cellP = $cellR = 'None';
  if (isset($object['MODS'])) {
    $count = 0;

    if ($node = $mx->query('/mods:mods/mods:extension/mods:creators')) {
      if (count($node) > 0) {
        foreach ($node as $n) {
          $cellP = $n->nodeValue;
        }
      } else {
        $cellP = 'None';
      }
    }

    if ($node = $mx->query('/mods:mods/mods:originInfo/mods:dateCreated')) {
      if (count($node) > 0) {
        foreach ($node as $n) {
          $cellR = $n->nodeValue;
        }
      } else {
        $cellR = 'None';
      }
    }
  }

  $data['P'] = $cellP;
  $data['R'] = $cellR;


  // Count the number of /mods:mods/mods:titleInfo[not(@*)] values, and the number of
  // /mods:mods/mods:titleInfo[@type='alternative'] values.
  /*
  $cell = '[0]';
  if (isset($object['MODS'])) {
    $mods = $object['MODS']->content;
    $count = 0;
    $xml = new SimpleXMLElement($mods);      // convert MODS content to searchable XML

    if ($node = $xml->xpath('/mods:mods/mods:titleInfo[not(@*)]')) {
      if (count($node) > 0) { $cell = "[".count($node)."]"; }
    }

    if ($node = $xml->xpath("/mods:mods/mods:titleInfo[@type='alternative']")) {
      if (count($node) > 0) { $cell .= " [".count($node)."]"; }
    }
  }
  $data['N'] = $cell;
  */

  // Commit $data to the database.
  ksort($data);
  or_set($pid, $data);

  // Return $toFollow if populated.
  if (count($toFollow) > 0) {
    return $toFollow;
  } else {
    return FALSE;
  }
}

/**
 * Count old (version 6) datastreams within a given object.
 *
 * @param $object
 * @param $count
 * @return array|bool
 */
function ihc_drush_count_old_content($object, &$count) {
  // The following are known derivative labels.  Don't count them!
  static $labels = array('pdf to swf','jpeg to swf','thumbnail','compressed mp4',
    'JPEG 2000,','Medium sized JPEG', 'compressed to mp3');
  $mimeCount = array( );
  $count = 0;

  foreach($object as $ds) {
    $id = $ds->id;
    if ($ds->state != 'A') { continue; }

    // If the DSID is not a known type... we have some work to do.
    if ($id != 'OBJ' && !in_array($id, icu_DSIDs( ))) {
      $label = $ds->label;
      if (!in_array($label, $labels)) {
        ++ $count;
        $mime = $ds->mimetype;
        if (empty($mimeCount[$mime])) {
          $mimeCount[$mime] = 1;
        } else {
          ++ $mimeCount[$mime];
        }
      }
    }
  }

  // Special... if the $mimeCount['jpeg'] === $mimeCount['tiff'] and $mimeCount['jpeg'] > 0 then we
  // shall assume the jpegs are unnecessary and should not be counted.  Remove them from the return.
  if (($mimeCount['image/jpeg'] > 0) && ($mimeCount['image/jpeg'] == $mimeCount['image/tiff'])) {
    $count -= $mimeCount['image/jpeg'];
    unset($mimeCount['image/jpeg']);
  }

  return ($count > 0 ? $mimeCount : FALSE);
}

/**
 * Fill a row of the worksheet ($filled) with existing data and format codes.
 *
 * Fill undefined/empty cells in the data with '[0] None' for reporting, and highlight gaps in numbering.
 *
 * @param array $columns
 *   Associative array of $col => $data pairs.
 * @param array $codes
 *   Associative array of $col => $code pairs.
 * @param array $reportColumns
 *   Array of up to 10 columns (letters) to be included.
 *
 * @return array
 */
function ihc_drush_fill_sheet($columns, $codes, $reportColumns=array('A','B','C','D','E','F','I','J','M','O')) {
  static $previous = 0;
  $filled = array( );

  // Use column 'A' to check for gaps in numbering.
  if ($previous == 0) {
    $codes['A'] = 'ORANGE';
  } else {
    $change = intval($columns['A']) - $previous;
    if ($change > 4) {
      $codes['A'] = 'ORANGE';
    } else if ($change > 1) {
      $codes['A'] = 'YELLOW';
    } else {
      unset($codes['A']);
    }
  }
  $previous = intval($columns['A']);

  // To overcome memory problems we will do this in parts.
  // Always print columns A, B, C as-is.
  // Up to 10 columns total can be reported.
  //
  $limit = ihcLimit( );

  // Now, apply the codes...
  $i = 0;
  foreach (range('A',$limit) as $c) {
    if (in_array($c,$reportColumns)) {
      if (empty($columns[$c])) {
        $filled[$i] = '%SOFT%[0] None';
      }
      else {
        if (isset($codes[$c])) {
          $filled[$i] = '%' . $codes[$c] . '%' . $columns[$c];
        }
        else {
          $filled[$i] = $columns[$c];
        }
      }
      ++$i;
    }
  }
  return $filled;
}

/**
 * Write a comment and/or follow-up iduF (or similar command) to a script for use later on.
 *
 * @param string $comment
 * @param string $command
 */
function ihc_drush_post_to_file($comment, $command, $path=NULL) {

  // if $path is NULL... Fetch the --script option.
  if (is_null($path)) {
    $script = drush_get_option('script', 'public://iduPendingOps.sh');
  } else {
    $script = $path;
  }

  // If there is a comment, prepend $comment with # and a timestamp.
  if (!is_null($comment)) {
    file_put_contents($script, "# $comment [" . date('Y-m-d H:i:s') . "] \n", FILE_APPEND);
  }

  // Now output the command as-is.
  if (!is_null($command)) {
    file_put_contents($script, "$command\n", FILE_APPEND);
  }
}


/**
 * Calculates a count of page visits to the page(s) associated with the specified object $pid.
 *
 * This function returns a count of page visits to the /drupal/islandora/object (was
 * /drupal/fedora/repository in DG6) page associated with the specified $pid object as well
 * as the estimated number of "downloads" associated with the object.
 * The function collects this information using a series of simple grep calls against the
 * AWStats aggregated data files found in /var/lib/awstats.
 *
 * @param string $pid - The target object PID.
 * @return array - Four integers, in order: 1) number of legacy DG6 visits, 2) number of DG7 visits,
 *                   3) number of DG6 downloads, and 4) number of DG7 downloads.
 */
function ihc_return_awstats($pid) {
  module_load_include('inc', 'idu', 'object_record');
  if (is_null($pid) || stripos($pid, ':') < 2) { return array(0, 0, 0, 0); }

  // OK, collect the data.
  $totals = array( );
  $totals[0] = $totals[1] = $totals[2] = $totals[3] = 0;

  list($pidp, $pidn) = explode(':', $pid);

  $divider = "\(:\|%3A\)";   // PID divider can be : or %3A.  This expression matches either.
  $record = array( );

  // Loop through all the years since November 2012 (112012).
  // Make a table named or_$pidp_awstats_$year for each year, plus one
  // 'or_$pidp_awstats_grandTotal' table.

  $thisYear = date("Y");
  $grandTable = "awstats_grandTotal";

  for ($y=2012, $yy=1; $y<=$thisYear; $y++, $yy++) {
    $year = sprintf('%04d', $y);
    $table = "awstats_".$year;

    // Loop through each month of the year...
    for ($m=1; $m<13; $m++) {
      $visits6 = $visits7 = $downloads6 = $downloads7 = $pdfDownloads6 = $normalDownloads6 = array( );
      $mnthYear = sprintf('%02d%04d', $m, $y);
      $f6 = "/var/lib/awstats/digital6/awstats".$mnthYear."*.txt";
      $awk6 = " [0-9]\+ [0-9]\+ [0-9]\+ [0-9]\+$' $f6 | awk '{ sum += $2 } END { print sum }'";
      $f7 = "/var/lib/awstats/awstats".$mnthYear."*.txt";
      $awk7 = " [0-9]\+ [0-9]\+ [0-9]\+ [0-9]\+$' $f7 | awk '{ sum += $2 } END { print sum }'";

      // Visits (in Islandora v6)
      if (count(glob($f6))) {
        $command = "grep '^/drupal/fedora/repository/" . $pidp . $divider . $pidn . $awk6;
        exec($command, $visits6);
      } else {
        $visits6[0] = 0;
      }

      // Visits (in Islandora v7)
      // The following line was only valid for OLD records before March 2015!
      //  $command = "grep '^/drupal7/islandora/object/".$pidp.$divider.$pidn.$awk7;
      if (count(glob($f7))) {
        $command = "grep '^/islandora/object/" . $pidp . $divider . $pidn . $awk7;
        exec($command, $visits7);
      } else {
        $visits7[0] = 0;
      }

      // Book AND generic downloads from v6
      if (count(glob($f6))) {
        $command = "grep '^/drupal/fedora/repository/" . $pidp . $divider . $pidn . "/PDF.\+" . $awk6;
        exec($command, $pdfDownloads6);

        $command = "grep '^/drupal/viewer/" . $pidp . $divider . $pidn . "/.\+/download" . $awk6;
        exec($command, $normalDownloads6);

        $downloads6[0] = $pdfDownloads6[0] + $normalDownloads6[0];
      } else {
        $downloads6[0] = 0;
      }

      // Downloads from v7
      // The following line was only valid for OLD records before March 2015!
      //  $command = "grep '^/drupal7/islandora/object/".$pidp.$divider.$pidn."\/download".$awk7;
      if (count(glob($f7))) {
        $command = "grep '^/islandora/object/" . $pidp . $divider . $pidn . "\/download" . $awk7;
        exec($command, $downloads7);
      } else {
        $downloads7[0] = 0;
      }

      // Add this data to the 'awstats' database if it is not all zeroes.
      if ($visits6[0] + $visits7[0] + $downloads6[0] + $downloads7[0] > 0) {
        ihc_add_awstats_to_record($m, $record, $visits6[0], $visits7[0], $downloads6[0], $downloads7[0]);
        or_set_awstats($pid, $table, $record);

        // Add the returned values to a running total.
        $totals[0] += $visits6[0];
        $totals[1] += $visits7[0];
        $totals[2] += $downloads6[0];
        $totals[3] += $downloads7[0];
      }
    }

    // Add the object's totals to the grand total table.  Column A=2012 views, B=2013 views...
    // Column AA=2012 downloads, AB=2013 downloads, etc.
    $rec = array( );
    ihc_add_awstats_to_record($yy, $rec, $totals[0], $totals[1], $totals[2], $totals[3]);
    or_set_awstats($pid, $grandTable, $rec);
  }

  return array($totals[0], $totals[1] + 1, $totals[2], $totals[3]);
}

/**
 * Helper function for ihc_return_awstats.
 *
 * @param int $m
 *   The month as a number from 1 to 12.
 * @param array $rec
 *   The target data record.
 * @param int $v6
 *   The number of version 6 visits during the month.
 * @param int $v7
 *   The number of version 7 visits during the month.
 * @param int $d6
 *   The number of version 6 downloads during the month.
 * @param int $d7
 *   The number of version 7 downloads during the month.
 * @return mixed
 *   The updated data record.
 */
function ihc_add_awstats_to_record($m, &$rec, $v6, $v7, $d6, $d7) {
  $column = range('A','Z');
  $mCol = $column[$m-1];
  $rec[$mCol] = $v6 + $v7;
  $dCol = 'A'.$mCol;
  $rec[$dCol] = $d6 + $d7;
  return $rec;
}
