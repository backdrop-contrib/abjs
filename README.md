# A/B Test JS

This module allows performant A/B and multivariate testing on your site via a
lightweight JS script. It is similar to Optimizely and Adobe Test&Target, but
focused more on flexibility and power and less on allowing non-technical folks
to setup tests.

## Installation

- Install this module using the [official Backdrop CMS instructions](https://backdropcms.org/guide/modules)

## Configuration

 * Configure user permissions in Configuration » User accounts » Permissions:

   - Administer A/B Test Configuration

     This permission allows one to create, edit, and delete Tests, but not
     to create, edit, or delete Conditions or Experiences, which require
     JavaScript.

   - Administer A/B Test Scripts and Settings

     This permission allows one to create, edit, and delete Experiences and
     and Conditions, as well as change the general settings of the module.

 * Customize the module settings at Configuration » User inteface » A/B Test JS.
   If Ace Code Editor is chosen, it will load via CDN on the Condition and
   Experience edit pages.

## Usage

- Usage instructions can be [viewed and edited in the Wiki](https://github.com/backdrop-contrib/abjs/wiki).
- [This Aten webinar](https://www.youtube.com/watch?v=5yV8gxIyS9w) (using the
  Drupal module) gives a good overview of the module and integration with
  analytics.

## Issues

 - Bugs and Feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/abjs/issues).

## Current Maintainers

 - [Laryn Kragt Bakker](https://github.com/laryn).
 - Collaboration and co-maintainers welcome!

## Credits

 - Current development is supported by [Aten Design Group](https://aten.io).
 - Ported to Backdrop CMS by [Laryn Kragt Bakker](https://github.com/laryn).
 - Maintained for Drupal 7 by [Matt Mowers](https://www.drupal.org/u/mmowers)
 and [Thalles Ferreira](https://www.drupal.org/u/thalles).

 ## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for
complete text.
