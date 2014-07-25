Commerce Beanstream Payment
===========================

This is a Drupal Commerce (http://drupalcommerce.org) Payment module
implementing simple one-time transactions with CCs in CAD for Beanstream
Payment processor (http://beanstream.com)

Features
========

* Basic Commerce Payment Gateway API implementation, for single-transaction Credit Card payments
(https://drupalcommerce.org/developer-guide/utilizing-core-apis/writing-payment-method-module)
* Card on File module integration, to allow users to store CC details using Beanstream Secure Payment Profiles
* Edit/Delete stored cards from /user profile page

Installation
============

You need to ensure you have the Phone (http://drupal.org/project/phone) module
installed on your site, and ensure you have at least one Phone field on your
Commerce Billing Profile.

Enable the module, and then configure a new payment method rule
(admin/commerce/config/payment-methods), and configure your merchant settings
in the Beanstream "action"
(admin/commerce/config/payment-methods/manage/commerce_payment_commerce_beanstream_payment/edit).
You will need:

* Merchant ID
* Username/Password (as setup in Beanstream account settings under "order
  settings") security/authentication for API - enables basic CC payments
  Passcode (as setup in Beanstream configuration under "payment profile
* configuration") - enables Card on File/Payment Profiles

(Module does not currently support other authentication mechanisms for the
basic API or Secure Payment Profiles)

You must also select which phone field the module should use to fill the
(required by Beanstream) phone number in transactions, and check which Credit
Card types your merchant account accepts.

You can optionally enable Card on File integration if you have the Card on File
module (http://drupal.org/project/commerce_cardonfile) installed. In this case,
you *must* have Secure Payment Profiles option enabled for your Beanstream
merchant account! If you don't, you will get API errors claiming "bad merchant
ID".
