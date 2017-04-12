#!/bin/env php
<?php
	$toc = array(
		'01.html' =>  'Getting Started (1)',
		'02.html' =>  'Selecting Text (2)',
		'03.html' =>  'Finding and Replacing Text (3)',
		'04.html' =>  'Cut and Paste (4)',
		'05.html' =>  'Using the Mouse (5)',
		'06.html' =>  'Keyboard Shortcuts (6)',
		'07.html' =>  'Shifting and Filling (7)',
		'08.html' =>  'Tabbed Editing (8)',
		'09.html' =>  'File Format (9)',
		'10.html' =>  'Programming with NEdit (10)',
		'11.html' =>  'Tab Stops/Emulated Tab Stops (11)',
		'12.html' =>  'Auto/Smart Indent (12)',
		'13.html' =>  'Syntax Highlighting (13)',
		'14.html' =>  'Finding Declarations(ctags) (14)',
		'15.html' =>  'Calltips (15)',
		'16.html' =>  'Basic Regular Expression Syntax (16)',
		'17.html' =>  'Metacharacters (17)',
		'18.html' =>  'Parenthetical Constructs (18)',
		'19.html' =>  'Advanced Topics (19)',
		'20.html' =>  'Example Regular Expressions (20)',
		'21.html' =>  'Shell Commands and Filters (21)',
		'22.html' =>  'Learn/Replay (22)',
		'23.html' =>  'Macro Language (23)',
		'24.html' =>  'Macro Subroutines (24)',
		'25.html' =>  'Rangesets (25)',
		'26.html' =>  'Highlighting Information (26)',
		'27.html' =>  'Action Routines (27)',
		'28.html' =>  'Customizing NEdit (28)',
		'29.html' =>  'Preferences (29)',
		'30.html' =>  'X Resources (30)',
		'31.html' =>  'Key Bindings (31)',
		'32.html' =>  'Highlighting Patterns (32)',
		'33.html' =>  'Smart Indent Macros (33)',
		'34.html' =>  'NEdit Command Line (34)',
		'35.html' =>  'Client/Server Mode (35)',
		'36.html' =>  'Crash Recovery(36)',
		'37.html' =>  'Version (37)',
		'38.html' =>  'GNU General Public License (38)',
		'39.html' =>  'Mailing Lists (39)',
		'40.html' =>  'Problems/Defects (40)',
		'41.html' =>  'Tabs Dialog (41)',
		'42.html' =>  'Customize Window Title Dialog (42)',
	);
	
	require("template/template.php"); 
	
	$index = 1;
	foreach ($toc as $key => $value) {
		ob_start();
		get_header($value);
		include("src/" . $key);
		get_footer(1, $index, 42);

		$htmlStr = ob_get_contents();
		ob_end_clean(); 
		file_put_contents("docs/" . $key, $htmlStr);
		
		
		$index += 1;
		
	}
