<?php

define('COMMERCE_BEANSTREAM_PAYMENT_URL', 'https://www.beanstream.com/scripts/process_transaction.asp');//SOAP URL https://www.beanstream.com/api/v1/payments
define('COMMERCE_BEANSTREAM_PAYMENT_PROFILE_URL', 'https://www.beanstream.com/scripts/payment_profile.asp');

function _commerce_beanstream_payment_create_profile($payment_method, $order, $pane_form, $pane_values, $billing) {
  // Determine the credit card type if possible for use in later code.
  if (!empty($pane_values['credit_card']['number'])) {
    module_load_include('inc', 'commerce_payment', 'includes/commerce_payment.credit_card');
    $card_type = commerce_payment_validate_credit_card_type($pane_values['credit_card']['number'], array_keys(commerce_payment_credit_card_types()));
  }

  $billing_address = $billing->commerce_customer_address->value();

  // Setup new Card entity
  $card = commerce_cardonfile_new();
  $card->uid = $order->uid;
  $card->payment_method = $payment_method['method_id'];
  $card->instance_id = $payment_method['instance_id'];
  $card->card_type = !empty($card_type) ? $card_type : 'card';
  $card->card_name = !empty($billing_address['name_line']) ? $billing_address['name_line'] : '';
  $card->card_exp_month = $pane_values['credit_card']['exp_month'];
  $card->card_exp_year = $pane_values['credit_card']['exp_year'];
  $card->card_code = $pane_values['credit_card']['code'];
  $card->status = 1;

  # Prepare request data
  $data = array(
    'trnOrderNumber' => $order->order_number,
    'trnCardOwner' => $card->card_name,
    'trnCardNumber' => $pane_values['credit_card']['number'],
    'trnExpMonth' => $card->card_exp_month,
    'trnExpYear' => substr($card->card_exp_year, -2),
    'trnCardCvd' => $card->card_code,
  );

  # Setup Billing details
  $phone_field = $payment_method['settings']['phone_field_name'];
  $phone_number = $billing->$phone_field->value();
  $data += array(
    'ordName' => $billing_address['name_line'],
    'ordAddress1' => $billing_address['thoroughfare'],
    'ordAddress2' => $billing_address['premise'],
    'ordCity' => $billing_address['locality'],
    'ordPostalCode' => $billing_address['postal_code'],
    'ordProvince' => $billing_address['administrative_area'],
    'ordCountry' => $billing_address['country'],
    'ordPhoneNumber' => $phone_number,
  );

  # Fill in email
  $u = user_load($order->uid);
  if( isset($u->mail) ) { $data['ordEmailAddress'] = $u->mail; }

  # Make the API request and check response
  $result = _commerce_beanstream_payment_profile_request('N', $payment_method, $data);
  if (empty($result) || $result['responseCode'] !== "1") {
    $error = array(
      '@code' => $result['responseCode'],
      '%message' => $result['responseMessage'],
    );

    if ($result['responseCode'] === "24") {
      // Try to ADD_CARD to existing profile
      if (isset($result['matchedCustomerCode'])) {

        $data['customerCode'] = $result['matchedCustomerCode'];
        $card_result = _commerce_beanstream_payment_profile_request('ADD_CARD', $payment_method, $data);

        if (!empty($card_result) && $card_result['responseCode'] === "1") {
          $card->card_number = substr($pane_values['credit_card']['number'], -4);
          $card->remote_id = $result['matchedCustomerCode'];
          #$card->card_exp_year = date("Y", strtotime(sprintf("%s-01-01", $orig_data['trnExpYear'])));
          commerce_cardonfile_save($card);
          return $card;
        } else {
          watchdog('commerce_beanstream', 'Beanstream Payment Profile error: '.$card_result['responseMessage']);
          return FALSE;
        }
      } else {
        watchdog('commerce_beanstream', 'Beanstream Payment Profile error: '.$result['responseMessage']);
        return FALSE;
      }

    } else {
      drupal_set_message(t('Payment Profile Creation failed: @code (%message)', $error), 'error');
      watchdog('commerce_beanstream', 'Beanstream Secure Payment Profile creation failed: @code', $error);
      return _commerce_beanstream_handle_errors($payment_method, $pane_form, $result, $phone_field);
    }
  } else { # Success
    // Save and log the creation of the new card on file.
    $card->card_number = substr($result['trnCardNumber'], -4);
    $card->remote_id = $result['customerCode'];
    commerce_cardonfile_save($card);
    watchdog('commerce_beanstream', 'Beanstream Secure Payment Profile @profile_id created and saved to user @uid.', array('@profile_id' => (string) $card->remote_id, '@uid' => $order->uid));
    return $card;
  }
}

function _commerce_beanstream_handle_errors($payment_method, $pane_form, $result, $phone_field) {
  $bill_prefix = 'customer_profile_billing[commerce_customer_address][und][0][';
  $prefix = implode('][', array_merge($pane_form['#array_parents'], array('credit_card'))) . '][';

  $fields_map = array(
    'trnCardOwner' => $bill_prefix . 'name_line',
    'ordName' => $bill_prefix . 'name_line',
    'trnCardNumber' => $prefix . 'number',
    'trnExpMonth' => $prefix . 'exp_month',
    'trnExpYear' => $prefix . 'exp_year',
    'trnCardCvd' => $prefix . 'code',
    'ordAddress1' => $bill_prefix . 'thoroughfare',
    'ordAddress2' => $bill_prefix . 'premise',
    'ordProvince' => $bill_prefix . 'administrative_area',
    'ordCountry' => $bill_prefix . 'country',
    'ordPostalCode' => $bill_prefix . 'postal_code',
    'ordPhoneNumber' => $bill_prefix . $phone_field,
    'ordEmailAddress' => '',
  );

  switch ($result['responseCode']) {
    case '2': # Failed card verification
      break;
    case '15': # Customer code does not exist
      break;
    case '16': # Customer code already exists
      break;
    case '17': # Duplicate match on payment info
      break;
    case '19': # Custom address/payment info failed validation
      watchdog('commerce_beanstream', 'Beanstream Payment Profile error: '.$result['errorMessage']);
      $error_fields = preg_split('/,/', $result['errorFields']);
      $error_msgs = preg_split('/<br>/', $result['errorMessage']);
      foreach ($error_fields as $i => $e) {
        form_set_error($fields_map[$e], $error_msgs[$i]);
      }
      return FALSE;
      break;
    case '24': # Duplicate match on billing information
      # Instead add a card to the existing profile, and then return it.
  }
}

/**
 * Helper function to make a Payment Profile Beanstream API request.
 *
 * @param string $op One of: N, M, ADD_CARD, Q, BACKEND
 * @param $payment_method
 * @param $data
 * @return bool|mixed
 */
function _commerce_beanstream_payment_profile_request($op, $payment_method, $data) {
  $beanstream_url = $payment_method['settings']['payment_profile_url'];

  $data += array(
    'operationType' => $op,
    'serviceVersion' => '1.1',
    'merchantId' => $payment_method['settings']['merchant_ID'],
    'passCode' => $payment_method['settings']['passcode'],
    'responseFormat' => 'QS', // QueryString format responses
  );

  $result = _commerce_beanstream_payment_curl($beanstream_url, $data);

  //parse response string into variables
  parse_str($result, $response);

  return $response;
}

/**
 * Helper function to make a Payment Profile Beanstream API request.
 *
 * @param string $op One of: N, M, ADD_CARD, Q, BACKEND
 * @param $payment_method
 * @param $data
 * @return bool|mixed
 */
function _commerce_beanstream_payment_profile_request_xml($op, $payment_method, $data) {
  $beanstream_url = $payment_method['settings']['payment_profile_url'];

  $data += array(
    'operationType' => $op,
    'serviceVersion' => '1.1',
    'merchantId' => $payment_method['settings']['merchant_ID'],
    'passCode' => $payment_method['settings']['passcode'],
    'responseFormat' => 'XML', // QueryString format responses
  );

  $result = _commerce_beanstream_payment_curl($beanstream_url, $data);

  return simplexml_load_string($result);
}

/**
 * Creates an example payment transaction for the specified charge amount.
 *
 * @param $payment_method
 *   Object The payment method instance object used to charge this payment.
 * @param $order
 *   Object The order object the payment applies to.
 * @param $charge
 *   Array indicating the amount and currency code to charge.
 * @param $pane_values
 *   Array of pane values entered on the submission form.
 * @return bool $transaction Object
 */
function commerce_beanstream_payment_transaction($payment_method, $order, $charge, $pane_values){
  //transaction info
  $transaction = commerce_payment_transaction_new('commerce_beanstream_payment', $order->order_id);
  $transaction->instance_id = $payment_method['instance_id'];
  $transaction->amount = $charge['amount'];
  $transaction->currency_code = $charge['currency_code'];
  $transaction->message = 'Name: @name';

  //build the request
  $request = array(
    'merchant_id' => $payment_method['settings']['merchant_ID'],
    'requestType' => 'BACKEND',
    'trnOrderNumber' => $order->order_id,
    'trnAmount' => number_format(commerce_currency_amount_to_decimal($transaction->amount,$transaction->currency_code),2),
    'trnCardNumber' => $pane_values['credit_card']['number'],
    'trnExpMonth' => $pane_values['credit_card']['exp_month'],
    'trnCardCvd' => $pane_values['credit_card']['code'],
    'trnExpYear' => substr($pane_values['credit_card']['exp_year'], -2),
    'trnType' => 'P',
  );

  //get billing address
  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  $phone_number_name = $payment_method['settings']['phone_field_name'];
  if(isset($order_wrapper->commerce_customer_billing)){
    $billing_address = $order_wrapper->commerce_customer_billing->commerce_customer_address->value();
    $billing_request = array();
    $billing_request['trnCardOwner'] = $billing_request['ordName'] = $billing_address['name_line'];
    $billing_request['ordAddress1'] = $billing_address['thoroughfare'];
    $billing_request['ordAddress2'] = $billing_address['premise'];
    $billing_request['ordCity'] = $billing_address['locality'];
    $billing_request['ordPostalCode'] = $billing_address['postal_code'];
    $billing_request['ordProvince'] = $billing_address['administrative_area'];
    $billing_request['ordCountry'] = $billing_address['country'];
    $billing_request['ordPhoneNumber'] = $order_wrapper->commerce_customer_billing->$phone_number_name->value();
    global $user;
    if(isset($user->mail)){
      $billing_request['ordEmailAddress'] = $user->mail;
    }
    $request = array_merge($request, $billing_request);
  }else{
    drupal_set_message(t('A billing address must be available to perform a transaction'), 'error');
  }
  //get shipping details if they exist
  if(isset($order_wrapper->commerce_customer_shipping)){
    $shipping_address = $order_wrapper->commerce_customer_shipping->commerce_customer_address->value();
    $shipping_request = array();
    $shipping_request['shipName'] = $shipping_address['name_line'];
    $shipping_request['shipAddress1'] = $shipping_address['thoroughfare'];
    $shipping_request['shipAddress2'] = $shipping_address['premise'];
    $shipping_request['shipCity'] = $shipping_address['locality'];
    $shipping_request['shipProvince'] = $shipping_address['administrative_area'];
    $shipping_request['shipCountry'] = $shipping_address['country'];
    $shipping_request['shipPostalCode'] = $shipping_address['postal_code'];
    $request = array_merge($request, $shipping_request);
  }

  //send request
  $response = commerce_beanstream_request($payment_method, $request);

  //if declined set the tranaction status to false
  if($response['trnApproved'] != '1'  || empty($response['trnApproved'])){
    $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
  }else{
    //otheriwse set transaction status appropriately
    // Set the transaction status based on the type of transaction this was.
    switch ($payment_method['settings']['txn_type']) {
      case COMMERCE_CREDIT_AUTH_ONLY:
        $transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
        break;

      case COMMERCE_CREDIT_AUTH_CAPTURE:
        $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
        break;
    }
  }

  //set tranaction variables to those returned from Beanstream
  $transaction->remote_id = $response['trnId'];
  $transaction->remote_status = $response['paymentMethod'];
  $transaction->payload[REQUEST_TIME] = $response;

  //set a msg to return to user
  $message = 'Your transacton has been <b>' . $response['messageText'] . '</b>';

  $transaction->message = $message;

  // save the transaction
  commerce_payment_transaction_save($transaction);

  // if the transaction is declined return false send error to user
  if($response['trnApproved'] != '1'){
    drupal_set_message(t('We received the following error processing your card. Please enter your information again or try a different card.'), 'error');
    drupal_set_message(check_plain($response['messageText']), 'error');
    return FALSE;
  }

  return $transaction;

}
/** send transaction request to beanstream to process */

function commerce_beanstream_request($payment_method, $request){
   $beanstream_url = $payment_method['settings']['payment_url'];

   $request += array(
     'username' => $payment_method['settings']['username'],
     'password' => $payment_method['settings']['password'],
     'merchant_id' => $payment_method['settings']['merchant_ID'],
     'requestType' => 'BACKEND',
     'responseFormat' => 'QS', // QueryString format responses
    );

  $result = _commerce_beanstream_payment_curl($beanstream_url, $request);

  //parse response string into variables
  parse_str($result, $response);

  return $response;
}

/**
 * Helper function to POST data to Beanstream URL using CURL
 *
 * @param string $url
 * @param array $data
 * @return bool|mixed Result of the transaction, or FALSE on error.
 */
function _commerce_beanstream_payment_curl($url, $data) {

  $ch = curl_init();                           // Initialize curl
  curl_setopt( $ch, CURLOPT_POST, 1 );         // Get curl to POST
  curl_setopt($ch, CURLOPT_SSL_VERIFYHOST,0);  // Disable SSL checks
  curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
  curl_setopt( $ch, CURLOPT_URL, $url);        // This is the location of the Online Mart payment gateway

  // Instruct curl to suppress the output from Online Mart, and to directly
  // return the transfer instead. (Output will be stored in $txResult.)
  curl_setopt( $ch, CURLOPT_RETURNTRANSFER, 1 );

  // These are the transaction parameters that we will POST
  curl_setopt( $ch, CURLOPT_POSTFIELDS,http_build_query($data));

  // Now POST the transaction. $txResult will contain Online Mart's response
  $txResult = curl_exec($ch);

  // Log any errors to the watchdog.
  if ($error = curl_error($ch)) {
    watchdog('commerce_beamstream_payment', 'cURL error: @error', array('@error' => $error), WATCHDOG_ERROR);
    return FALSE;
  }
  curl_close( $ch );

  // Return the result as-is, since the responseFormat may be different
  return $txResult;
}

