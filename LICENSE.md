# Antares Simulation Engine and ADG License Agreement

**Identifier: ASE-ADG-LIC-2026-10-16-1.0**  
**Issue date: 16 October 2026**  
**Contracting licensor: ANTARES SIMULACAO LTDA — CNPJ 69.363.366/0001-21**

This is a custom license. It is not the MIT License, the Apache License or an unrestricted open-source license. Public source access does not remove the contribution and commercial conditions below. No backend subscription is required merely to download, fork, use or develop Public Components under this Agreement.

## 1. Parties, authority and scope

1.1 “Licensor” means ANTARES SIMULACAO LTDA, with its registered office at Rua Milton de Brito Firmeza 180, Icaraí, Caucaia, Ceará, CEP 61624-365, Brazil. “You” means the individual or legal entity accepting this Agreement. A person acting for an entity must identify that entity and have authority to bind it. Related entities are not automatically the same licensee.

1.2 The Licensor contracts in its own name. Jan Ohlmann retains the rights privately held by him and is the founder and concept author. Naming him in copyright notices or as a project approval authority does not make him personally responsible for the Licensor's customer services. The Licensor's authority to license his private materials derives from a separate agreement; this document does not itself transfer those materials to the company.

1.3 This Agreement applies to materials expressly released under its identifier. A license file in a parent repository does not replace an independent license in a subrepository or third-party dependency. The Licensor grants only rights it validly holds. Statutory rights and rights independently obtained under other licenses are not removed.

1.4 “ADG” means the Antares Developer Group project community, not a separate legal person merely because of its name. ADG members are the individuals and entities admitted under the Project Participation Agreement, `PJV.md`, identifier ASE-ADG-PJV-2026-10-16-1.0. Mere public use does not require or create membership.

1.5 The project concerns the Antares Simulation Engine (“Engine”) and its ecosystem. Antares Open World is a project within that ecosystem. Authorized use is not confined to that particular game.

## 2. Components, extensions and your own products

2.1 “Components” include the Engine code and associated modules, plugins, interface definitions, build materials and documentation expressly covered by this Agreement. Independently licensed materials retain their licenses and notices.

2.2 An “Ecosystem Extension” is a modification of a Component, or a new module or plugin extending, adapting or supplying Engine functionality, including through the Engine API to a client. This includes a module written exclusively for your own product and a new implementation containing no copied Engine source. The obligation to contribute such an extension is a contractual condition; it does not depend on whether the extension is a copyright derivative work.

2.3 Your “Product” is your own game, client application or tool. Its independent source, interface, game content, assets and product-specific logic remain yours or their respective owners'. Merely calling an API does not make an entire Product a contribution. Conversely, placing an Engine module inside a client repository or executable does not exempt that module. Functional purpose, not naming or packaging, determines the boundary. An independent pre-existing library is not automatically contributed merely because it is used; any necessary permissions must still be available.

2.4 For example, a landscape editor remains its developer's Product. An erosion module developed for it and connected through the Engine API is an Ecosystem Extension and must be contributed. The editor's independent source need not be disclosed.

2.5 A “Contribution” is an actual development state submitted through the project contribution process, including initial, incomplete and intermediate versions. The term does not automatically include every work a contributor may create in the future.

## 3. Acceptance and release information

3.1 This Agreement is offered with the relevant Components. You must have an opportunity to read it before accepting it. You accept by an express electronic acknowledgment or, where legally effective, by knowingly exercising the permissions offered here after receiving the terms. A bare download is not treated as conclusive proof that every contractual charge was knowingly accepted. Access or platform rights independently provided by GitHub or mandatory law remain unaffected.

3.2 Before private access or your first contribution, the project process records your identity, any represented entity, the LIC and PJV versions and the approved repository or task. An authorized initial commit and push after the conditions have been presented can record acceptance without a paper agreement for each commit. A commit author's name alone is not proof of authority to license an employer's code.

3.3 Commercial conditions in sections 9–11 must be presented and accepted before monetization. An acknowledgment naming this license version is sufficient; no negotiation of an additional royalty rate is required. Public download does not subscribe you to PaaS.

3.4 The applicable repository or component notice identifies public or restricted release, any separate notices and, for restricted access, the authorization defining recipients and duration. An authorized public release under this Agreement is a “Public Component.” A leak, mistaken access setting or unauthorized upload is not a public release. A later change of repository visibility does not revoke rights already granted to lawfully received Public Components.

## 4. Public permissions

4.1 Subject to this Agreement, the Licensor grants you a worldwide, non-exclusive license to download, clone, fork, copy, study, execute, modify and develop Public Components, and to use them in your own Products. These permissions are not conditioned on buying or maintaining a backend subscription. They remain available after cancellation or downgrade, subject to compliance with this Agreement.

4.2 You may redistribute Public Components, including modified versions, with this Agreement, accurate copyright and attribution notices, and applicable third-party notices intact. Recipients must receive the applicable license terms. You may not represent private Components as publicly released or impose terms that prevent recipients exercising permissions granted by the actual licensor. Your fork does not replace the project contribution duties in section 6.

4.3 Public development may be noncommercial or commercial. Product reporting and royalties apply as specified below. No individual permission is required for each compliant use of Public Components, apart from the prior project approval required for new contributed modules and plugins. Public reuse by a person who is not an ADG member does not grant access to private ADG repositories.

4.4 Subject to compliance, public permissions are granted for the duration of the relevant intellectual-property rights. Discontinuing distribution or membership does not by itself cancel them. This Agreement is not a promise of future versions, free hosting, support or acceptance of a contribution.

## 5. Restricted and Enterprise permissions

5.1 A restricted Component may be used only within its documented authorization. A community authorization can arise from an approved project role; an Enterprise authorization arises from the relevant customer agreement. Neither grants access to every private repository.

5.2 The authorized customer may allow its properly authorized personnel and contractors to work for it within that authorization, subject to confidentiality and license compliance. This is not an independent license to those persons, affiliates, publishers or the customer's customers. A separately identified personal community role must not be used to circumvent an employer's or customer's restrictions.

5.3 An Enterprise authorization permits the specified inspection, development and adaptation of private code during its authorized term. Possession of a downloaded copy does not extend that term. Redistributing private source, exposing it through a public fork, giving another business independent Engine development rights or selling an Engine sublicense is not authorized by an ordinary Enterprise subscription.

5.4 This Agreement does not grant a right to operate a dedicated Engine server or independent Engine PaaS service. Such operating rights require an express separate grant. Authorized client-side execution and offline simulation parity are not, merely for that reason, dedicated-server operation. Independent rights under a different license are unaffected.

## 6. Approval and continuous contribution

6.1 Before starting a new Ecosystem Extension, submit its purpose, scope and proposed module or plugin boundary for approval by Jan Ohlmann or a project authority expressly authorized by him. Existing-component work must follow the recorded task and review process. Approval considers the Engine's simulation purpose, published project standards and communicated ethical restrictions. No right exists to force arbitrary content into the project.

6.2 Once approved, the extension must be initialized in its assigned project repository and linked into the prescribed superrepository/subrepository structure. Authorized credentials are provided through the access process. Development must be visible there from its initial state, not only when finished or commercially released.

6.3 You must commit and push actual development states continuously as part of the development workflow. Local editing remains possible, but withholding the development process in an internal fork until a final delivery is not compliance. Company forks and mirrors are allowed within their access permissions but do not replace the canonical contribution repository. No unannounced daily or hourly deadline is imposed by this clause.

6.4 Contributions must follow the applicable documented architecture, coding, interface, versioning and documentation standards, and include the editable source, README, necessary build and integration information and license notices. The applicable standard is the one communicated for the approved work; a later standard does not silently rewrite past obligations.

6.5 Company, employee, individual and community contributions follow the same rules. Internal use, lack of sales and integration only into your own Product do not exempt an Ecosystem Extension. Products themselves remain excluded under section 2.

6.6 Make contributions available to the recipients authorized for that project repository. This does not require publishing restricted Engine code to the internet. Jan, the Licensor and the ADG members authorized for the relevant Component must be able to obtain and use the contributed development under section 7.

6.7 Do not commit passwords, production keys, personal customer data or third-party secrets. Supply safe configuration information instead. Security-sensitive work must use the authorized protected project channel. A platform outage must be reported and accumulated work synchronized when access is restored; deliberate withholding is not an outage.

6.8 Main-branch integration is a separate review decision. The contribution grant applies to actual authorized submissions, including intermediate states, without waiting for a merge. Approval or merge cannot legalize an unauthorized third-party submission.

## 7. Contribution rights and continuing availability

7.1 For each Contribution you are authorized to submit, you grant non-exclusive, worldwide rights for its applicable protection term to copy, execute, study, modify, combine, develop, reproduce, distribute within the authorized release scope, commercially exploit and sublicense it for the Engine ecosystem and compliant Products. No separate contribution royalty is owed merely because these rights are exercised. Employment compensation and separately agreed remuneration remain unaffected.

7.2 The grant expressly benefits Jan Ohlmann personally, the Licensor and the ADG members authorized for the Component. They may accept and enforce this benefit to the extent permitted by applicable law. Jan's benefit is independent of continued operation of the Licensor. Granting these contribution rights does not make him the provider of your customer's services.

7.3 Jan may use Contributions in his private Engine development and authorize other Engine operators; the Licensor may use them in its authorized commercial Engine, backend and Product offerings; authorized ADG members may use them within the ecosystem under this Agreement. Private access boundaries still apply. Required Product runtime permissions may be passed to end users under section 9. Authorized operators must respect contribution availability, independent Product ownership and existing recipient rights.

7.4 The grant includes continuation and relicensing under section 15. You cannot revoke granted Contribution rights solely because you leave, become inactive, change employer, downgrade, cancel or disagree with subsequent development. You need not create new Contributions after ending your involvement. Mandatory statutory rights remain unaffected.

7.5 You retain ownership of your independently owned rights. This Agreement does not claim that Jan authored your work, assign him all future works or transfer the Engine's ownership to the Licensor. Accurate attribution and protected author rights remain intact. It grants the specific permissions required for continued shared use.

7.6 To the extent you may grant them, the rights include necessary patent and utility-model permissions to use, make, have made, modify, distribute and supply the Contribution and authorized combinations. No new patent charge or withdrawal arises merely from departure. If you lack necessary authority, disclose the limitation before submission and do not submit incompatible material.

7.7 Rights in included marks, images and other materials extend only to their authorized use with the Contribution. They do not allow general appropriation of an entity's branding, identity or endorsement. License changes on departure to AGPL, Creative Commons or another license are not triggered by this Agreement.

## 8. Provenance, third-party materials and responsibility

8.1 You must own or hold sufficient permissions for the rights you grant, including relevant employment, contracting, patent and third-party rights. This applies to AI-assisted work as well. Disclose external dependencies and conditions before integration and preserve required notices. Do not submit materials whose obligations conflict with the intended authorized use without resolving that conflict first.

8.2 You are responsible under applicable law for your attributable breaches. To the extent lawful, you indemnify affected beneficiaries for substantiated third-party claims and reasonable necessary defense costs caused by your culpable breach of the submission and permission requirements. The affected party must promptly notify you, allow reasonable participation in the defense and avoid binding admissions or settlements without appropriate consultation. Its own misconduct is not shifted to you.

8.3 No clause binds an outside rightsholder or transfers criminal responsibility. A submission, a copyright header or the Licensor's review is not a cure for missing rights. Disputed material may be isolated pending investigation without ending the rights in unrelated lawful contributions.

## 9. Products, publication and customer distribution

9.1 You retain your independent Product. That exemption from source contribution does not exempt the Product from reporting and royalties. For this Agreement a Product subject to those conditions is one incorporating Components or using their Engine functionality at runtime, locally or through the backend. Merely using a development utility to create an otherwise independent output does not by itself make that output a royalty-bearing Product.

9.2 You may distribute compliant Products and give end users the installation and runtime permissions necessary to use them. Private Engine runtime may be included only in the executable or other runtime form expressly designated as distributable in the Component release; private development source and standalone development rights are excluded. Public source remains redistributable under section 4. End users do not acquire Enterprise access by purchasing your Product.

9.3 Publishers, app stores and distributors may distribute the Product on your behalf within your rights. A platform condition requiring broader Engine rights must be resolved before you accept that condition. No agency relationship, endorsement or personal warranty by Jan is implied.

9.4 Before first external Product availability, notify the Licensor of your identity, Product, distribution channels, intended date, relevant Components and monetization. This includes external alpha, beta, testing, early-access and full-launch versions. Internal development alone is not external release. Update the report for a material change of Product identity, channels or monetization; ordinary version updates do not need a newly negotiated license.

9.5 Before first monetization, record acceptance of the commercial provisions in sections 10–11. Paid alpha/beta access, early access and qualifying presales are not deferred until an official launch. A compliant report and acceptance do not require the Licensor to exercise a discretionary veto over otherwise permitted Public Component use.

9.6 You remain responsible for your Product, its users, content and customer promises. Keep required notices accessible in an appropriate legal-information location. Nothing grants authority to promise hosting or support on behalf of Jan personally.

## 10. Royalty schedule

10.1 The royalty rate is **5%** of Royalty Revenue. Each Product has one **USD 100,000 lifetime revenue allowance**. It does not reset annually, on a change of platform, a version update, a new publisher or transfer of the same Product to another entity. Revenue above the allowance is chargeable; crossing it does not retroactively charge the exempt amount.

10.2 Product Revenue means worldwide receipts attributable to the Product: sales, paid access, subscriptions, in-Product transactions, advertising, paid alpha/beta and early access, and presales or crowdfunding consideration for Product access or benefits. Product-related advances are counted to the extent attributable to those receipts, without charging the same amount again on recoupment. Unrelated business revenue and genuine donations without Product consideration are excluded.

10.3 Calculate receipts before platform commissions, publisher shares and development or operating expenses. Deduct separately identified transaction taxes actually payable to public authorities and genuine customer refunds. Reversals must be evidenced and allocated to the Product, without double deductions. A later refund of previously royalty-bearing revenue is credited against that Product's royalty account; a remaining credit on final settlement is refundable rather than silently forfeited.

10.4 All platform versions and updates of the same Product are combined. Genuinely independent Products have independent allowances. Bundles and expansions must be allocated on a reasonable, documented and consistently applied basis reflecting their economic relationship. Do not fragment or rename one Product to multiply allowances. Material allocation uncertainty must be disclosed with the report.

10.5 A calendar quarter with Product Revenue **below USD 1,000** incurs no royalty. That revenue still counts toward the lifetime allowance. At exactly USD 1,000 or above, this quarterly exemption does not apply. It is an exemption threshold, not an additional USD 1,000 deduction.

10.6 Otherwise Royalty Revenue is the quarter's Product Revenue less the unused lifetime allowance at the quarter's start, but never below zero. The unused allowance equals USD 100,000 less prior cumulative Product Revenue, but never below zero. Refund corrections must be reconciled so that neither receipts nor allowance usage are counted twice.

10.7 Examples: with USD 80,000 prior cumulative revenue and USD 30,000 in the quarter, the royalty is USD 500. Once the allowance is exhausted, a USD 800 quarter incurs no royalty; a USD 2,000 quarter incurs USD 100. These examples assume no later corrections.

10.8 Backend fees are independent of royalties and are not automatically credited against them. No per-install charge, new royalty rate, special store discount or additional compulsory public-code subscription is created by this schedule.

## 11. Reporting, currency and payment

11.1 From first monetization, report quarterly Product Revenue, deductions, cumulative revenue, allowance usage and royalties within **45 calendar days after quarter end**. Pay any royalty within the same period. A no-payment report must show why no payment is due. An inactive Product can be marked inactive with its final report; report renewed monetization when it resumes.

11.2 Keep relevant statements and supporting records for the applicable statutory retention periods. The Licensor may request reasonable evidence of an identified discrepancy. Any review must protect confidential business and personal data, remain confined to relevant revenue and not demand your exempt Product source. Wider audits require a separately agreed procedure.

11.3 USD is the reference unit for the thresholds and calculations. For BRL receipts use the Banco Central do Brasil USD/BRL PTAX closing selling rate for the receipt date, or the latest preceding published business-day rate. For another currency, use an evidenced actual USD settlement rate where available; otherwise use the corresponding published central-bank reference conversion consistently and disclose the source and method. Do not select rates opportunistically between transactions.

11.4 For settlement required in BRL, convert the USD amount using the BCB closing selling rate on the business day immediately preceding invoice issuance. The Licensor supplies lawful invoicing and payment instructions in time for payment. Mandatory currency, tax and withholding requirements prevail. Lawfully required withholding supported by official evidence is accounted for; no automatic tax gross-up is imposed by this Agreement.

11.5 Submit reports through the designated private customer channel or the confidential contact supplied at onboarding. In the absence or failure of an electronic channel, notice can be delivered to the registered office in section 1. Never disclose private revenue in a public GitHub issue. An access failure must not be used to manufacture default when timely notice was reasonably attempted through an available alternative.

11.6 Correct discovered errors without undue delay. Late reporting does not automatically erase the lifetime allowance. Actual breaches remain subject to section 14 and applicable law. A new website price does not create a retroactive royalty.

## 12. PaaS and Enterprise services

12.1 PaaS purchases backend provision and the contracted portal, database, API and hosting resources. Standard customers use the authorized portal and interfaces; Enterprise customers additionally receive their specified private-source rights. The binding order identifies the purchased resources, prices, billing period, support and service scope. This Agreement is not itself a hosting order.

12.2 Service availability, technical validity information, data handling, export, backup and retention must be supplied in the customer terms and order as applicable. No private license or support commitment is inferred solely from a tariff label. Mandatory duties relating to software commercialization remain unaffected.

12.3 An order may limit the private development rights it supplies. It must not extinguish the public permissions in section 4 solely because a subscription ends. A customer remains entitled to develop an otherwise compliant Product using only independently licensed Public Components after cancellation.

## 13. Cancellation, downgrade and existing releases

13.1 You may request downgrade at any time, effective at the end of the current agreed billing period. A monthly period ends at its agreed month end and an annual period at its agreed annual end. The order and mandatory law govern termination notice and any refund; the ability to request downgrade does not shorten the purchased period.

13.2 At downgrade, the new tier's rights and resources apply. Private rights no longer included cease prospectively. At cancellation or suspension, the affected backend and access stop as provided by the contract. Possessing a private checkout creates neither continued development rights nor a self-hosting license.

13.3 You may continue distributing the unchanged, lawfully released Product version within surviving distribution rights. End users may continue its authorized runtime use. Royalties and reporting remain applicable. This does not promise continued backend services: an offline Product may function independently, whereas a backend-dependent Product may cease to function.

13.4 You may not create a new Product or version using expired private development rights, including a purported maintenance update that needs those rights. You may resume with a suitable authorization or develop within rights you still hold. Work using only lawfully available Public Components remains permitted without subscription under section 4.

13.5 Retain private copies only as allowed for surviving distribution rights, required records or authorized archives. Retention does not authorize new development. Apply the relevant return/deletion duties without destroying records mandatory law requires. Subscription or membership termination does not withdraw rights already granted in Contributions.

## 14. Breach, warranties and responsibility

14.1 The Licensor may require cessation and correction of an identified breach. A remediable breach normally receives a reasonable cure opportunity stated with the notice, proportionate to its seriousness and the work required. Immediate targeted access suspension may be used where necessary for serious unauthorized disclosure, infringement or security risks. Termination for cause follows applicable law; a minor technical mistake does not automatically terminate every license.

14.2 Contributions, confidentiality, accrued reporting/payment duties and valid existing third-party rights survive to the extent of their terms. Section 13 is not a guarantee that an unlawfully distributed Product may continue after termination for serious infringement.

14.3 Free development materials are supplied in their existing state without a contractual promise of fitness, completion or error-free operation unless expressly given. Paid service promises and mandatory rights are unaffected. You must assess suitability for your intended Product.

14.4 To the extent permitted by law, liability for indirect or consequential losses and lost profits is excluded. This does not exclude fraud, intentional or grossly negligent conduct, personal injury, a binding guarantee, mandatory liability or liability that cannot lawfully be excluded for essential contractual duties. It does not override consumer protection. No arbitrary financial liability cap is imposed here.

14.5 Jan does not personally guarantee the company's debts or customer contracts, and no service duty transfers to him automatically on company failure. Each party remains responsible for its own legally attributable conduct. This Agreement cannot prevent all third-party claims or displace mandatory statutory liability.

## 15. Versions and project continuity

15.1 Jan Ohlmann is the designated issuer of future LIC versions. Each must have an identifier, date and change record. Contribution permissions include authority to continue and sublicense actual submitted Contributions under later versions issued by him that preserve shared Engine development, availability to authorized community recipients, the independent Product exclusion and already granted contribution benefits. A contributor's further signature is not required for uses within that validly granted authority.

15.2 This authority does not impose arbitrary new payment, liability or performance duties on existing users. Changes to their contracts require a lawful basis. Silence or someone else's commit is not blanket consent. Already granted permissions and independent third-party licenses cannot be removed retroactively by replacing a file.

15.3 The ADG and valid Contribution rights are not dissolved simply because one operating company ceases business. Company control changes do not convey Jan's private ownership or an automatic right to future versions. The separate upstream license and mandatory insolvency rules govern the company's position. Another operator needs its own authority; this Agreement gives no general franchise entitlement.

## 16. PJV, notices and governing law

16.1 Contributors must also accept the identified PJV. The LIC governs contribution rights, royalty terms, public/private permissions and Product boundaries; the PJV governs participation and project workflow. Neither document silently amends the company's articles of association. For newly accepted contributions under these versions, inconsistent withdrawal and automatic-license-switch rules in the 2016 LIC/PJV do not apply. This does not retroactively rewrite an old contract merely by publication.

16.2 Individual terms may vary matters within the parties' actual authority, but cannot dispose of another rightsholder's rights. Personal data processing requires its own lawful basis and necessary privacy information. This is not a substitute for a data-processing agreement where one is required.

16.3 Notices must be reasonably evidenced in writing or electronic text through the relevant private contractual channel or registered office. Ordinary public-code issues can use repository channels. Technical project approval is not a personal guarantee or a waiver of monetary terms.

16.4 Brazilian law applies to the extent a choice of law is effective. Mandatory protections applicable to a person or transaction, and non-waivable court jurisdiction, remain applicable. This choice of law does not deprive a consumer of the protection afforded by the mandatory provisions of the law of the State of the consumer's habitual residence. Where a forum agreement is lawful, the courts of Caucaia, Ceará, Brazil are designated. No arbitration requirement or waiver of mandatory collective remedies is imposed.

16.5 This Agreement is provided in Brazilian Portuguese (pt-BR), English and German with the same content. The pt-BR version is controlling; if the language versions differ, the pt-BR version prevails. The English and German versions are always supplied together with the pt-BR version before contracting and are published with it on the Licensor's website. Where applicable law requires Portuguese or other understandable precontractual information, the Licensor must supply it before contracting. No language priority defeats mandatory interpretation or information duties, including consumer-protection rules of the State of habitual residence. A translation must identify its corresponding version and any expressly agreed controlling status.

16.6 If a clause is invalid, the remaining provisions survive only to the extent permitted by law. An invalid exclusion is not automatically rewritten to the maximum restrictive effect possible. Applicable statutory replacement rules govern.

16.7 This version becomes an offer for a Component only upon authorized publication after the Licensor has the necessary rights. Its issue date is not an assertion that an upstream agreement has already been signed. Unaltered copies may accompany licensed material. Unapproved modifications must not be presented as an official Antares license.

Copyright in the license text: © 2026 Jan Ohlmann. Copyright in licensed materials remains with the respective rightsholders.
