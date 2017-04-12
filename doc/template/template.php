<?php
function get_header($title) {
print <<<END
<!DOCTYPE html>
<html lang="en">
<head>
	<title>$title</title>
	<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css" integrity="sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u" crossorigin="anonymous">
	<style>
		dt {
			margin-top:1em;
		}
		body {
			margin-top: 1em;
		}
		/*
		ul {
    		list-style:none;
    		padding-left:0;
		}
		*/
	</style>
</head>
<body>
<div class="container">
	<div class="row">
		<div class="col-md-12">

END;
}


function get_footer($min, $curr, $max) {


$prev_url = ($curr > $min) ? sprintf('%02d.html', $curr - 1) : sprintf('%02d.html', $curr);
$next_url = ($curr < $max) ? sprintf('%02d.html', $curr + 1) : sprintf('%02d.html', $curr);

print <<<END
		</div>
	</div>
	<div class="row">
		<div class="col-md-12">
			<footer>
				<nav class="nav">				
					<a class="nav-link" href="$prev_url">Prev</a>
					<a class="nav-link" href="$next_url">Next</a>
				</nav>
			</footer>
		</div>
	</div>
</div>
</body>
</html>

END;
}
