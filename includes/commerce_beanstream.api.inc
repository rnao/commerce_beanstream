<?php
/**
 * Creates an example payment transaction for the specified charge amount.
 *
 * @param Object $payment_method
 *   The payment method instance object used to charge this payment.
 * @param Object $order
 *   The order object the payment applies to.
 * @param Array $charge
 *   An array indicating the amount and currency code to charge.
 * @param Array $pane_values
 *   The array of pane values entered on the submission form.
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
  $txnResponse = commerce_beanstream_request($payment_method, $request);

  //parse response string into variables
  parse_str($txnResponse, $response);

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
    );

  // Initialize curl
  $ch = curl_init();
  // Get curl to POST
  curl_setopt( $ch, CURLOPT_POST, 1 );
  curl_setopt($ch, CURLOPT_SSL_VERIFYHOST,0); curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
  // Instruct curl to suppress the output from Online Mart, and to directly
  // return the transfer instead. (Output will be stored in $txResult.)
  curl_setopt( $ch, CURLOPT_RETURNTRANSFER, 1 );
  // This is the location of the Online Mart payment gateway
  curl_setopt( $ch, CURLOPT_URL, $beanstream_url);
  // These are the transaction parameters that we will POST
  curl_setopt( $ch, CURLOPT_POSTFIELDS,http_build_query($request));
// Now POST the transaction. $txResult will contain Online Mart's response
  $txResult = curl_exec( $ch );
  // Log any errors to the watchdog.
  if ($error = curl_error($ch)) {
    watchdog('commerce_beamstream_payment', 'cURL error: @error', array('@error' => $error), WATCHDOG_ERROR);
    return FALSE;
  }
  curl_close( $ch );
  return $txResult;
  // Make the response an array and trim off the encapsulating characters.
}

