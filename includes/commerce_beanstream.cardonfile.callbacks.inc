<?php

/**
* Card on File callback: create new card
*
* @param array $form
* @param array $form_state
* @param object $payment_method
* @param array $card
*
* @return array Profile info, with at least a 'remote_id' key
*/
function commerce_beanstream_cardonfile_create($form, $form_state, $payment_method, $card) {
dd($form, "cardonfile_create form");
dd($form_state, "cardonfile_create form_state");
dd($payment_method, "cardonfile_create payment_method");
dd($card, "cardonfile_create card");
}

/**
* Card on file callback: background charge payment
*
* @param object $payment_method
*  The payment method instance definition array.
* @param object $card_data
*   The stored credit card data array to be processed
* @param object $order
*   The order object that is being processed
* @param array $charge
*   The price array for the charge amount with keys of 'amount' and 'currency'
*   If null the total value of the order is used.
*
* @return void TRUE if the transaction was successful
*/
function commerce_beanstream_cardonfile_charge($payment_method, $card_data, $order, $charge = NULL) {
// Format order total for transaction.
if (!isset($charge)) {
$wrapper = entity_metadata_wrapper('commerce_order', $order);
$charge = commerce_line_items_total($wrapper->commerce_line_items);
}

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

}

/**
* Card on file callback: update card information
*
* @param array $form
*   The payment form
* @param array $form_state
*   The payment form state
* @param object $payment_method
*   The payment method instance definition array
* @param object $card_data
*   The new credit card data array to be processed
*/
function commerce_beanstream_cardonfile_update($form, &$form_state, $payment_method, $card_data){}

/**
* Card on file callback: delete card information
*
* @param array $form
*   The payment form
* @param array $form_state
*   The payment form state
* @param object $payment_method
*   The payment method instance definition array
* @param object $card_data
*   The credit card data to be deleted
*/
function commerce_beanstream_cardonfile_delete($form, &$form_state, $payment_method, $card_data){}
