--TEST--
Table out variations
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$tt->vanish();

$rec = $tt->put(null, array('test' => 'abc123111'));

var_dump($tt->get($rec));

$tt->out($rec);
echo "success\n";

var_dump($tt->get($rec));

$tt->out($rec);
echo "success\n";

$rec = array();

$rec[] = $tt->put(null, array('test' => 'abc123111'));
$rec[] = $tt->put(null, array('test' => 'abc123111'));
$rec[] = $tt->put(null, array('test' => 'abc123111'));
$rec[] = $tt->put(null, array('test' => 'abc123111'));
$rec[] = $tt->put(null, array('test' => 'abc123111'));

var_dump($tt->get($rec[3]));

$tt->out($rec);
echo "success\n";

echo "here comes null:\n";
var_dump($tt->get($rec[3]));

?>
--EXPECT--
array(1) {
  ["test"]=>
  string(9) "abc123111"
}
success
NULL
success
array(1) {
  ["test"]=>
  string(9) "abc123111"
}
success
here comes null:
NULL